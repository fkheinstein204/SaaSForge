import http from 'k6/http';
import { check, sleep } from 'k6';
import { Rate, Trend } from 'k6/metrics';

// Custom metrics
const loginSuccessRate = new Rate('login_success');
const loginDuration = new Trend('login_duration');
const jwtValidationDuration = new Trend('jwt_validation_duration');
const logoutSuccessRate = new Rate('logout_success');

// Test configuration
export const options = {
  scenarios: {
    // Scenario 1: Gradual ramp-up to test system capacity
    ramp_up: {
      executor: 'ramping-vus',
      startVUs: 0,
      stages: [
        { duration: '2m', target: 50 },   // Ramp to 50 users
        { duration: '5m', target: 50 },   // Stay at 50 users
        { duration: '2m', target: 100 },  // Ramp to 100 users
        { duration: '5m', target: 100 },  // Stay at 100 users
        { duration: '2m', target: 0 },    // Ramp down
      ],
    },

    // Scenario 2: Spike test (NFR-A4: 1000 concurrent logins)
    spike_test: {
      executor: 'constant-vus',
      vus: 1000,
      duration: '1m',
      startTime: '16m',  // Run after ramp_up
    },

    // Scenario 3: Soak test (sustained load)
    soak_test: {
      executor: 'constant-vus',
      vus: 50,
      duration: '30m',
      startTime: '17m',  // Run after spike_test
    },
  },

  thresholds: {
    // NFR-P1: Login should complete in ≤500ms (p95)
    'http_req_duration{endpoint:login}': ['p(95)<500'],

    // NFR-P1a: JWT validation should complete in ≤2ms (p95)
    'http_req_duration{endpoint:validate}': ['p(95)<2'],

    // Success rates
    'login_success': ['rate>0.99'],  // 99% login success
    'logout_success': ['rate>0.99'], // 99% logout success

    // Error rates
    'http_req_failed': ['rate<0.01'], // <1% error rate
  },
};

// Base URL
const BASE_URL = __ENV.API_URL || 'http://localhost:8000';

// Test data
const testUsers = [
  { email: 'loadtest1@example.com', password: 'LoadTest123!' },
  { email: 'loadtest2@example.com', password: 'LoadTest123!' },
  { email: 'loadtest3@example.com', password: 'LoadTest123!' },
  { email: 'loadtest4@example.com', password: 'LoadTest123!' },
  { email: 'loadtest5@example.com', password: 'LoadTest123!' },
];

// Get random test user
function getRandomUser() {
  return testUsers[Math.floor(Math.random() * testUsers.length)];
}

export function setup() {
  console.log('🚀 Starting Auth Service Load Test');
  console.log(`   Base URL: ${BASE_URL}`);
  console.log(`   Test Users: ${testUsers.length}`);
  return { startTime: new Date().toISOString() };
}

export default function () {
  const user = getRandomUser();

  // Test 1: Login
  const loginPayload = JSON.stringify({
    email: user.email,
    password: user.password,
  });

  const loginParams = {
    headers: { 'Content-Type': 'application/json' },
    tags: { endpoint: 'login' },
  };

  const loginStart = Date.now();
  const loginRes = http.post(`${BASE_URL}/v1/auth/login`, loginPayload, loginParams);
  const loginEnd = Date.now();

  const loginSuccess = check(loginRes, {
    'login status is 200': (r) => r.status === 200,
    'login returns access_token': (r) => JSON.parse(r.body).access_token !== undefined,
    'login returns refresh_token': (r) => JSON.parse(r.body).refresh_token !== undefined,
    'login has expires_in': (r) => JSON.parse(r.body).expires_in === 900,
  });

  loginSuccessRate.add(loginSuccess);
  loginDuration.add(loginEnd - loginStart);

  if (!loginSuccess) {
    console.error(`Login failed: ${loginRes.status} - ${loginRes.body}`);
    return; // Skip remaining tests if login failed
  }

  const loginData = JSON.parse(loginRes.body);
  const accessToken = loginData.access_token;
  const refreshToken = loginData.refresh_token;

  sleep(0.1); // 100ms think time

  // Test 2: Validate Token
  const validateParams = {
    headers: {
      'Authorization': `Bearer ${accessToken}`,
    },
    tags: { endpoint: 'validate' },
  };

  const validateStart = Date.now();
  const validateRes = http.get(`${BASE_URL}/v1/auth/validate`, validateParams);
  const validateEnd = Date.now();

  check(validateRes, {
    'validate status is 200': (r) => r.status === 200,
    'validate returns valid=true': (r) => JSON.parse(r.body).valid === true,
    'validate has user_id': (r) => JSON.parse(r.body).user_id !== undefined,
    'validate has tenant_id': (r) => JSON.parse(r.body).tenant_id !== undefined,
  });

  jwtValidationDuration.add(validateEnd - validateStart);

  sleep(0.5); // 500ms think time (simulating user activity)

  // Test 3: Create API Key
  const apiKeyPayload = JSON.stringify({
    name: `k6-test-key-${Date.now()}`,
    scopes: ['read', 'write'],
  });

  const apiKeyParams = {
    headers: {
      'Authorization': `Bearer ${accessToken}`,
      'Content-Type': 'application/json',
    },
    tags: { endpoint: 'create_api_key' },
  };

  const apiKeyRes = http.post(`${BASE_URL}/v1/auth/api-keys`, apiKeyPayload, apiKeyParams);

  check(apiKeyRes, {
    'create_api_key status is 200': (r) => r.status === 200,
    'create_api_key returns key': (r) => {
      const body = JSON.parse(r.body);
      return body.api_key && body.api_key.startsWith('sk_');
    },
  });

  sleep(0.2); // 200ms think time

  // Test 4: Logout
  const logoutPayload = JSON.stringify({
    refresh_token: refreshToken,
  });

  const logoutParams = {
    headers: {
      'Authorization': `Bearer ${accessToken}`,
      'Content-Type': 'application/json',
    },
    tags: { endpoint: 'logout' },
  };

  const logoutRes = http.post(`${BASE_URL}/v1/auth/logout`, logoutPayload, logoutParams);

  const logoutSuccess = check(logoutRes, {
    'logout status is 200': (r) => r.status === 200,
    'logout returns success': (r) => JSON.parse(r.body).success === true,
  });

  logoutSuccessRate.add(logoutSuccess);

  sleep(1); // 1s think time between iterations
}

export function teardown(data) {
  console.log('✅ Auth Service Load Test Complete');
  console.log(`   Started: ${data.startTime}`);
  console.log(`   Ended: ${new Date().toISOString()}`);
}

export function handleSummary(data) {
  return {
    'stdout': textSummary(data, { indent: '→', enableColors: true }),
    'tests/load/results/auth_load_test_summary.json': JSON.stringify(data),
  };
}

function textSummary(data, options = {}) {
  const indent = options.indent || '';
  const enableColors = options.enableColors || false;

  let summary = '\n';
  summary += `${indent}═══════════════════════════════════════════════════════\n`;
  summary += `${indent}      Auth Service Load Test Results\n`;
  summary += `${indent}═══════════════════════════════════════════════════════\n\n`;

  // Requests
  const requests = data.metrics.http_reqs.values.count;
  const failed = data.metrics.http_req_failed.values.rate * 100;
  summary += `${indent}Requests:\n`;
  summary += `${indent}  Total:        ${requests}\n`;
  summary += `${indent}  Failed:       ${failed.toFixed(2)}%\n`;
  summary += `${indent}  Success:      ${(100 - failed).toFixed(2)}%\n\n`;

  // Response times
  const duration = data.metrics.http_req_duration.values;
  summary += `${indent}Response Times:\n`;
  summary += `${indent}  Avg:          ${duration.avg.toFixed(2)}ms\n`;
  summary += `${indent}  Min:          ${duration.min.toFixed(2)}ms\n`;
  summary += `${indent}  Max:          ${duration.max.toFixed(2)}ms\n`;
  summary += `${indent}  p50:          ${duration['p(50)'].toFixed(2)}ms\n`;
  summary += `${indent}  p95:          ${duration['p(95)'].toFixed(2)}ms\n`;
  summary += `${indent}  p99:          ${duration['p(99)'].toFixed(2)}ms\n\n`;

  // Thresholds
  summary += `${indent}Thresholds:\n`;
  for (const [name, threshold] of Object.entries(data.thresholds || {})) {
    const passed = threshold.passes === threshold.thresholds;
    const status = passed ? '✓' : '✗';
    summary += `${indent}  ${status} ${name}\n`;
  }

  summary += `\n${indent}═══════════════════════════════════════════════════════\n`;

  return summary;
}
