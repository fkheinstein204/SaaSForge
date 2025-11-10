/**
 * Load test: API performance under load
 * Using k6 for load testing
 */
import http from 'k6/http';
import { check, sleep } from 'k6';

export const options = {
  stages: [
    { duration: '30s', target: 20 },   // Ramp up to 20 users
    { duration: '1m', target: 100 },   // Ramp up to 100 users
    { duration: '2m', target: 100 },   // Stay at 100 users
    { duration: '30s', target: 0 },    // Ramp down to 0 users
  ],
  thresholds: {
    http_req_duration: ['p(95)<2000'], // 95% of requests should be below 2s
    http_req_failed: ['rate<0.01'],    // Error rate should be below 1%
  },
};

const BASE_URL = 'http://localhost:8000';

export default function () {
  // Test login endpoint
  const loginPayload = JSON.stringify({
    email: 'admin@demo.com',
    password: 'password123',
  });

  const loginRes = http.post(`${BASE_URL}/v1/auth/login`, loginPayload, {
    headers: { 'Content-Type': 'application/json' },
  });

  check(loginRes, {
    'login status is 200 or 501': (r) => r.status === 200 || r.status === 501,
  });

  // Test health endpoint
  const healthRes = http.get(`${BASE_URL}/health`);

  check(healthRes, {
    'health status is 200': (r) => r.status === 200,
    'health response time < 100ms': (r) => r.timings.duration < 100,
  });

  sleep(1);
}
