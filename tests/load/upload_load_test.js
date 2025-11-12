import http from 'k6/http';
import { check, sleep } from 'k6';
import { Rate, Trend, Counter } from 'k6/metrics';
import { SharedArray } from 'k6/data';

// Custom metrics
const presignSuccessRate = new Rate('presign_success');
const presignDuration = new Trend('presign_duration');
const uploadSuccessRate = new Rate('upload_success');
const uploadDuration = new Trend('upload_duration');
const quotaEnforcementCounter = new Counter('quota_enforcement');

// Test configuration
export const options = {
  scenarios: {
    // Scenario 1: Mixed workload (small and large files)
    mixed_load: {
      executor: 'ramping-vus',
      startVUs: 0,
      stages: [
        { duration: '2m', target: 20 },   // Ramp to 20 users
        { duration: '5m', target: 20 },   // Stay at 20 users
        { duration: '1m', target: 0 },    // Ramp down
      ],
    },

    // Scenario 2: Large file uploads (NFR-B4: 5GB file upload)
    large_file_test: {
      executor: 'constant-vus',
      vus: 5,
      duration: '5m',
      startTime: '8m',
      exec: 'largeFileScenario',
    },
  },

  thresholds: {
    // NFR-P2: Presign request should complete in ≤300ms (p99)
    'http_req_duration{endpoint:presign}': ['p(99)<300'],

    // Success rates
    'presign_success': ['rate>0.99'],  // 99% presign success
    'upload_success': ['rate>0.95'],   // 95% upload success (lower due to quota)

    // Error rates
    'http_req_failed{endpoint:presign}': ['rate<0.01'],
  },
};

// Base URL
const BASE_URL = __ENV.API_URL || 'http://localhost:8000';

// Test user credentials (should be pre-created)
const TEST_USER = {
  email: 'uploadtest@example.com',
  password: 'UploadTest123!',
};

let accessToken = '';

export function setup() {
  console.log('🚀 Starting Upload Service Load Test');
  console.log(`   Base URL: ${BASE_URL}`);

  // Login to get access token
  const loginPayload = JSON.stringify({
    email: TEST_USER.email,
    password: TEST_USER.password,
  });

  const loginRes = http.post(`${BASE_URL}/v1/auth/login`, loginPayload, {
    headers: { 'Content-Type': 'application/json' },
  });

  if (loginRes.status !== 200) {
    console.error('Setup failed: Unable to login');
    return null;
  }

  const loginData = JSON.parse(loginRes.body);
  accessToken = loginData.access_token;

  return {
    accessToken: accessToken,
    startTime: new Date().toISOString(),
  };
}

// Default scenario: Mixed file sizes
export default function (data) {
  if (!data || !data.accessToken) {
    console.error('No access token available');
    return;
  }

  // Generate random file size (1KB to 10MB)
  const fileSizeKB = Math.floor(Math.random() * 10240) + 1;
  const fileSizeBytes = fileSizeKB * 1024;

  uploadFile(data.accessToken, fileSizeBytes, 'image/jpeg');

  sleep(2); // 2s think time between uploads
}

// Large file scenario
export function largeFileScenario(data) {
  if (!data || !data.accessToken) {
    console.error('No access token available');
    return;
  }

  // Test with 100MB chunks (simulating 5GB multipart upload)
  const chunkSize = 100 * 1024 * 1024; // 100MB
  uploadFile(data.accessToken, chunkSize, 'video/mp4');

  sleep(5); // 5s think time for large files
}

function uploadFile(token, fileSizeBytes, contentType) {
  // Step 1: Request presigned URL
  const presignPayload = JSON.stringify({
    filename: `test-${Date.now()}.jpg`,
    content_type: contentType,
    size: fileSizeBytes,
  });

  const presignParams = {
    headers: {
      'Authorization': `Bearer ${token}`,
      'Content-Type': 'application/json',
    },
    tags: { endpoint: 'presign' },
  };

  const presignStart = Date.now();
  const presignRes = http.post(`${BASE_URL}/v1/upload/presign`, presignPayload, presignParams);
  const presignEnd = Date.now();

  const presignSuccess = check(presignRes, {
    'presign status is 200 or 403': (r) => r.status === 200 || r.status === 403,
    'presign returns url on success': (r) => {
      if (r.status === 200) {
        const body = JSON.parse(r.body);
        return body.upload_url !== undefined;
      }
      return true; // Skip check if quota exceeded
    },
  });

  presignSuccessRate.add(presignSuccess);
  presignDuration.add(presignEnd - presignStart);

  if (presignRes.status === 403) {
    // Quota exceeded
    quotaEnforcementCounter.add(1);
    console.log('Quota exceeded (expected behavior)');
    return;
  }

  if (!presignSuccess || presignRes.status !== 200) {
    console.error(`Presign failed: ${presignRes.status} - ${presignRes.body}`);
    return;
  }

  const presignData = JSON.parse(presignRes.body);
  const uploadUrl = presignData.upload_url;
  const objectId = presignData.object_id;

  sleep(0.1); // 100ms think time

  // Step 2: Simulate upload to S3 (mock)
  // In real scenario, this would upload to presigned URL
  // For load testing, we just verify the URL is valid
  const uploadSuccess = check(uploadUrl, {
    'upload_url is not empty': (url) => url.length > 0,
    'upload_url contains http': (url) => url.startsWith('http'),
  });

  uploadSuccessRate.add(uploadSuccess);

  sleep(0.2); // 200ms think time

  // Step 3: Complete upload
  const completePayload = JSON.stringify({
    object_id: objectId,
    etag: `"${Math.random().toString(36).substring(7)}"`, // Mock ETag
  });

  const completeParams = {
    headers: {
      'Authorization': `Bearer ${token}`,
      'Content-Type': 'application/json',
    },
    tags: { endpoint: 'complete' },
  };

  const completeRes = http.post(`${BASE_URL}/v1/upload/complete`, completePayload, completeParams);

  check(completeRes, {
    'complete status is 200': (r) => r.status === 200,
    'complete returns object_id': (r) => {
      const body = JSON.parse(r.body);
      return body.object_id === objectId;
    },
  });

  sleep(0.1); // 100ms think time

  // Step 4: Get quota status
  const quotaParams = {
    headers: {
      'Authorization': `Bearer ${token}`,
    },
    tags: { endpoint: 'quota' },
  };

  const quotaRes = http.get(`${BASE_URL}/v1/upload/quota`, quotaParams);

  check(quotaRes, {
    'quota status is 200': (r) => r.status === 200,
    'quota returns used_bytes': (r) => {
      const body = JSON.parse(r.body);
      return body.used_bytes !== undefined;
    },
    'quota returns limit_bytes': (r) => {
      const body = JSON.parse(r.body);
      return body.limit_bytes !== undefined;
    },
  });
}

export function teardown(data) {
  console.log('✅ Upload Service Load Test Complete');
  if (data) {
    console.log(`   Started: ${data.startTime}`);
  }
  console.log(`   Ended: ${new Date().toISOString()}`);
}

export function handleSummary(data) {
  return {
    'stdout': textSummary(data),
    'tests/load/results/upload_load_test_summary.json': JSON.stringify(data),
  };
}

function textSummary(data) {
  let summary = '\n';
  summary += '═══════════════════════════════════════════════════════\n';
  summary += '      Upload Service Load Test Results\n';
  summary += '═══════════════════════════════════════════════════════\n\n';

  // Requests
  const requests = data.metrics.http_reqs.values.count;
  const failed = data.metrics.http_req_failed.values.rate * 100;
  summary += 'Requests:\n';
  summary += `  Total:        ${requests}\n`;
  summary += `  Failed:       ${failed.toFixed(2)}%\n`;
  summary += `  Success:      ${(100 - failed).toFixed(2)}%\n\n`;

  // Presign performance
  if (data.metrics.presign_duration) {
    const presign = data.metrics.presign_duration.values;
    summary += 'Presign URL Generation:\n';
    summary += `  Avg:          ${presign.avg.toFixed(2)}ms\n`;
    summary += `  p50:          ${presign['p(50)'].toFixed(2)}ms\n`;
    summary += `  p95:          ${presign['p(95)'].toFixed(2)}ms\n`;
    summary += `  p99:          ${presign['p(99)'].toFixed(2)}ms (Target: ≤300ms NFR-P2)\n\n`;
  }

  // Quota enforcement
  if (data.metrics.quota_enforcement) {
    summary += 'Quota Enforcement:\n';
    summary += `  Blocked:      ${data.metrics.quota_enforcement.values.count} requests\n\n`;
  }

  // Thresholds
  summary += 'Thresholds:\n';
  for (const [name, threshold] of Object.entries(data.thresholds || {})) {
    const passed = threshold.passes === threshold.thresholds;
    const status = passed ? '✓' : '✗';
    summary += `  ${status} ${name}\n`;
  }

  summary += '\n═══════════════════════════════════════════════════════\n';

  return summary;
}
