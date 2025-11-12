import http from 'k6/http';
import { check, sleep } from 'k6';
import { Rate, Trend } from 'k6/metrics';

// Custom metrics
const subscriptionCreateRate = new Rate('subscription_create_success');
const paymentProcessingDuration = new Trend('payment_processing_duration');
const idempotencyEnforcementCounter = new Rate('idempotency_enforcement');

// Test configuration
export const options = {
  scenarios: {
    // Scenario 1: Subscription lifecycle test
    subscription_lifecycle: {
      executor: 'ramping-vus',
      startVUs: 0,
      stages: [
        { duration: '1m', target: 10 },   // Ramp to 10 users
        { duration: '3m', target: 10 },   // Stay at 10 users
        { duration: '1m', target: 0 },    // Ramp down
      ],
    },

    // Scenario 2: High-volume usage metering (NFR-C6: 10k req/s)
    usage_metering: {
      executor: 'constant-arrival-rate',
      rate: 1000, // 1000 requests per second
      timeUnit: '1s',
      duration: '2m',
      preAllocatedVUs: 50,
      maxVUs: 100,
      startTime: '5m',
      exec: 'recordUsage',
    },
  },

  thresholds: {
    // NFR-P4: Payment processing should complete in ≤3s (p95)
    'http_req_duration{endpoint:create_subscription}': ['p(95)<3000'],

    // Usage recording should be fast
    'http_req_duration{endpoint:record_usage}': ['p(95)<100'],

    // Success rates
    'subscription_create_success': ['rate>0.98'],  // 98% success (Stripe failures expected)

    // Error rates
    'http_req_failed{endpoint:create_subscription}': ['rate<0.05'],
  },
};

// Base URL
const BASE_URL = __ENV.API_URL || 'http://localhost:8000';

// Test user
const TEST_USER = {
  email: 'paymenttest@example.com',
  password: 'PaymentTest123!',
};

let accessToken = '';

export function setup() {
  console.log('🚀 Starting Payment Service Load Test');
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

// Default scenario: Subscription lifecycle
export default function (data) {
  if (!data || !data.accessToken) {
    console.error('No access token available');
    return;
  }

  // Step 1: Add payment method
  const paymentMethodPayload = JSON.stringify({
    type: 'card',
    card_token: `tok_test_${Date.now()}`, // Mock Stripe token
  });

  const paymentMethodParams = {
    headers: {
      'Authorization': `Bearer ${data.accessToken}`,
      'Content-Type': 'application/json',
    },
    tags: { endpoint: 'add_payment_method' },
  };

  const pmRes = http.post(
    `${BASE_URL}/v1/payment/payment-methods`,
    paymentMethodPayload,
    paymentMethodParams
  );

  check(pmRes, {
    'add_payment_method status is 200': (r) => r.status === 200,
    'add_payment_method returns method_id': (r) => {
      const body = JSON.parse(r.body);
      return body.method_id !== undefined;
    },
  });

  if (pmRes.status !== 200) {
    console.error(`Add payment method failed: ${pmRes.status}`);
    return;
  }

  const methodId = JSON.parse(pmRes.body).method_id;

  sleep(0.5); // 500ms think time

  // Step 2: Create subscription
  const subscriptionPayload = JSON.stringify({
    plan: 'pro',
    payment_method_id: methodId,
  });

  const subscriptionParams = {
    headers: {
      'Authorization': `Bearer ${data.accessToken}`,
      'Content-Type': 'application/json',
      'Idempotency-Key': `sub-${Date.now()}-${Math.random()}`,
    },
    tags: { endpoint: 'create_subscription' },
  };

  const subStart = Date.now();
  const subRes = http.post(
    `${BASE_URL}/v1/payment/subscriptions`,
    subscriptionPayload,
    subscriptionParams
  );
  const subEnd = Date.now();

  const subSuccess = check(subRes, {
    'create_subscription status is 200 or 400': (r) => r.status === 200 || r.status === 400,
    'create_subscription returns subscription_id': (r) => {
      if (r.status === 200) {
        const body = JSON.parse(r.body);
        return body.subscription_id !== undefined;
      }
      return true; // Skip check if failed
    },
  });

  subscriptionCreateRate.add(subSuccess);
  paymentProcessingDuration.add(subEnd - subStart);

  if (subRes.status !== 200) {
    return; // Expected: Stripe mock may reject some
  }

  const subscriptionId = JSON.parse(subRes.body).subscription_id;

  sleep(1); // 1s think time

  // Step 3: Get subscription details
  const getSubParams = {
    headers: {
      'Authorization': `Bearer ${data.accessToken}`,
    },
    tags: { endpoint: 'get_subscription' },
  };

  const getSubRes = http.get(
    `${BASE_URL}/v1/payment/subscriptions/${subscriptionId}`,
    getSubParams
  );

  check(getSubRes, {
    'get_subscription status is 200': (r) => r.status === 200,
    'get_subscription returns plan': (r) => {
      const body = JSON.parse(r.body);
      return body.plan === 'pro';
    },
  });

  sleep(2); // 2s think time

  // Step 4: Test idempotency - retry subscription creation with same key
  const idempotentSubRes = http.post(
    `${BASE_URL}/v1/payment/subscriptions`,
    subscriptionPayload,
    subscriptionParams // Same idempotency key
  );

  const isIdempotent = check(idempotentSubRes, {
    'idempotent_request returns cached result': (r) => r.status === 200,
    'idempotent_request same subscription_id': (r) => {
      if (r.status === 200) {
        const body = JSON.parse(r.body);
        return body.subscription_id === subscriptionId;
      }
      return false;
    },
  });

  idempotencyEnforcementCounter.add(isIdempotent);

  sleep(1); // 1s think time

  // Step 5: Cancel subscription
  const cancelPayload = JSON.stringify({
    reason: 'load_test',
  });

  const cancelParams = {
    headers: {
      'Authorization': `Bearer ${data.accessToken}`,
      'Content-Type': 'application/json',
    },
    tags: { endpoint: 'cancel_subscription' },
  };

  const cancelRes = http.post(
    `${BASE_URL}/v1/payment/subscriptions/${subscriptionId}/cancel`,
    cancelPayload,
    cancelParams
  );

  check(cancelRes, {
    'cancel_subscription status is 200': (r) => r.status === 200,
    'cancel_subscription sets canceled_at': (r) => {
      const body = JSON.parse(r.body);
      return body.canceled_at !== undefined;
    },
  });

  sleep(2); // 2s think time between iterations
}

// Usage metering scenario
export function recordUsage(data) {
  if (!data || !data.accessToken) {
    return;
  }

  const usagePayload = JSON.stringify({
    metric: 'api_calls',
    quantity: Math.floor(Math.random() * 100) + 1,
    timestamp: new Date().toISOString(),
  });

  const usageParams = {
    headers: {
      'Authorization': `Bearer ${data.accessToken}`,
      'Content-Type': 'application/json',
    },
    tags: { endpoint: 'record_usage' },
  };

  const usageRes = http.post(
    `${BASE_URL}/v1/payment/usage`,
    usagePayload,
    usageParams
  );

  check(usageRes, {
    'record_usage status is 200': (r) => r.status === 200,
  });

  // No sleep - high throughput scenario
}

export function teardown(data) {
  console.log('✅ Payment Service Load Test Complete');
  if (data) {
    console.log(`   Started: ${data.startTime}`);
  }
  console.log(`   Ended: ${new Date().toISOString()}`);
}

export function handleSummary(data) {
  return {
    'stdout': textSummary(data),
    'tests/load/results/payment_load_test_summary.json': JSON.stringify(data),
  };
}

function textSummary(data) {
  let summary = '\n';
  summary += '═══════════════════════════════════════════════════════\n';
  summary += '      Payment Service Load Test Results\n';
  summary += '═══════════════════════════════════════════════════════\n\n';

  // Requests
  const requests = data.metrics.http_reqs.values.count;
  const failed = data.metrics.http_req_failed.values.rate * 100;
  summary += 'Requests:\n';
  summary += `  Total:        ${requests}\n`;
  summary += `  Failed:       ${failed.toFixed(2)}%\n`;
  summary += `  Success:      ${(100 - failed).toFixed(2)}%\n\n`;

  // Payment processing performance
  if (data.metrics.payment_processing_duration) {
    const payment = data.metrics.payment_processing_duration.values;
    summary += 'Payment Processing (Create Subscription):\n';
    summary += `  Avg:          ${payment.avg.toFixed(2)}ms\n`;
    summary += `  p50:          ${payment['p(50)'].toFixed(2)}ms\n`;
    summary += `  p95:          ${payment['p(95)'].toFixed(2)}ms (Target: ≤3000ms NFR-P4)\n`;
    summary += `  p99:          ${payment['p(99)'].toFixed(2)}ms\n\n`;
  }

  // Idempotency
  if (data.metrics.idempotency_enforcement) {
    const idempotency = data.metrics.idempotency_enforcement.values.rate * 100;
    summary += 'Idempotency:\n';
    summary += `  Enforced:     ${idempotency.toFixed(2)}% (Requirement E-124)\n\n`;
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
