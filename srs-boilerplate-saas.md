# Software Requirements Specification (SRS)
## Security & Upload Subsystems

---

**Project:** SaaSForge (C++ gRPC → FastAPI BFF → React/TS)  
**Version:** 0.3 (complete)  
**Date:** 2025-11-10  
**Subsystems:** A. Authentication & Roles, B. Upload, C. Payment & Subscriptions, D. Notifications, E. Email & Transactions  
**Authentication Architecture:** Hybrid JWT + mTLS (see [Authentication Architecture Document](srs-boilerplate-recommendation-implementation.md))

---

## 1. Introduction

### 1.1 Purpose

Define the product vision and verifiable requirements for SaaSForge — a production-ready, multi-tenant SaaS boilerplate that accelerates teams from prototype to launch. SaaSForge provides secure identity, file ingestion and transformation, subscription billing, multi-channel notifications, and operational observability out of the box, built on a C++ gRPC core with a FastAPI BFF and a React/TypeScript UI.

Primary objectives:
- Provide opinionated defaults with modular, provider-agnostic integrations (S3/R2, Stripe/PayPal, SendGrid/SES).
- Embed security and privacy by design (RBAC/ABAC, 2FA/TOTP, encryption in transit/at rest, audit trails, GDPR/PCI-DSS alignment).
- Enable robust multi-tenancy with isolation, quotas, rate limits, and usage metering for fair billing.
- Deliver measurable reliability and operability via SLOs, metrics, tracing, and structured logging.
- Define deterministic APIs and contracts, including idempotency, consistent error codes, and testable acceptance criteria.

The remainder of this document specifies verifiable requirements and acceptance tests for five subsystems: A. Authentication & Roles, B. Upload, C. Payment & Subscriptions, D. Notifications, and E. Email & Transactions.

### 1.2 Scope

The framework exposes:

- **C++ gRPC core services** (Auth, Identity, Upload, Transform, Payment, Notification)
- **FastAPI BFF** proxying/aggregating gRPC and adding HTTP auth, rate limiting, and presigned-URL brokering
- **React/TS UI** for user flows (auth, device/keys management, uploads, billing, notifications, status)

### 1.3 Definitions & Abbreviations

| Term | Definition |
|------|------------|
| **TOTP** | Time-based One-Time Password (RFC 6238) |
| **RBAC** | Role-Based Access Control |
| **ABAC** | Attribute-Based Access Control (e.g., tenantId, region, plan) |
| **BFF** | Back-End for Front-End |
| **PII** | Personally Identifiable Information |
| **RTM** | Requirements Traceability Matrix |
| **SLO** | Service Level Objective |
| **RPO** | Recovery Point Objective |
| **RTO** | Recovery Time Objective |
| **STRIDE** | Spoofing, Tampering, Repudiation, Information Disclosure, Denial of Service, Elevation of Privilege |
| **PCI-DSS** | Payment Card Industry Data Security Standard |
| **SCA** | Strong Customer Authentication (3D Secure 2.0) |
| **MRR** | Monthly Recurring Revenue |
| **ARR** | Annual Recurring Revenue |
| **JWT** | JSON Web Token (RFC 7519) |
| **mTLS** | Mutual Transport Layer Security |
| **JTI** | JWT ID (unique identifier for token) |
| **KID** | Key ID (identifies signing key) |

---

## 1.4 Authentication Architecture Overview

SaaSForge implements a **three-tier hybrid authentication strategy** combining JWT and mTLS for optimal security and performance:

### Tier 1: User Authentication (JWT-Based)
- **OAuth 2.0/OIDC** integration (Google, GitHub, Microsoft)
- **Email/Password** authentication with 2FA (TOTP/OTP)
- **JWT tokens** with RS256 signing (15-minute access tokens, 30-day refresh tokens)
- **Redis blacklist** for instant token revocation
- **Refresh token rotation** with reuse detection

### Tier 2: BFF to gRPC Services (JWT Validation)
- FastAPI BFF validates JWT signatures and checks Redis blacklist
- Validated JWT passed to C++ gRPC services via metadata
- C++ services perform additional JWT validation and extract user context
- Authorization enforcement at both BFF and service layers

### Tier 3: Service-to-Service (mTLS)
- **Mutual TLS** for internal gRPC service communication
- Certificate-based authentication (no tokens needed)
- Automatic certificate rotation via cert-manager (Kubernetes)
- Strongest security for backend service mesh

**For detailed implementation, see:** [Authentication Architecture Document](srs-boilerplate-recommendation-implementation.md)

---

## 1.5 Key Libraries & Technologies

| Component | Technology | Purpose |
|-----------|------------|---------|
| **JWT Generation/Validation** | jwt-cpp (C++), PyJWT (Python) | Token signing and verification |
| **gRPC mTLS** | gRPC C++ built-in, gRPC Python | Service-to-service authentication |
| **Token Blacklist** | Redis (redis-plus-plus C++, redis-py) | Instant token revocation |
| **Key Management** | AWS KMS, Azure Key Vault | JWT signing key storage |
| **Certificate Management** | cert-manager (Kubernetes) | Automatic mTLS cert rotation |

---

## 2. System Context

### 2.1 Actors

- **End User** (Browser/React)
- **Service Account / API Client** (server-to-server via API keys)
- **Tenant Admin** (manages roles, policies)
- **System Operator** (observability, incident response)

### 2.2 External Systems

- **Email/SMS provider** (OTP, notifications)
- **S3-compatible object storage** (AWS S3 or Cloudflare R2)
- **KMS** (AWS KMS/Azure Key Vault) for key management and envelope encryption
- **OIDC IdP** (optional) for enterprise SSO
- **Payment Gateway** (Stripe, PayPal, Adyen)
- **Email Service Provider** (SendGrid, AWS SES, Postmark)
- **SMS Gateway** (Twilio, AWS SNS, Vonage)
- **Push Notification Service** (FCM, APNs, OneSignal)
- **Webhook Delivery Service** (Svix, Hookdeck)

---

## 3. Interfaces

### 3.1 FastAPI BFF (HTTPS/JSON)

Representative endpoints (tenant-scoped unless noted):

#### Authentication Endpoints
- `POST /v1/auth/login`
- `POST /v1/auth/logout`
- `POST /v1/auth/totp/setup`
- `POST /v1/auth/totp/verify`
- `POST /v1/auth/otp/send`
- `POST /v1/auth/otp/verify`
- `POST /v1/auth/password/change`
- `POST /v1/auth/email/change/initiate`
- `POST /v1/auth/email/change/confirm`

#### Session Management
- `GET /v1/sessions`
- `DELETE /v1/sessions/{sessionId}`

#### API Keys
- `POST /v1/api-keys`
- `GET /v1/api-keys`
- `DELETE /v1/api-keys/{keyId}`

#### Roles
- `GET /v1/roles`
- `POST /v1/roles`
- `POST /v1/roles/{id}/assign`

#### Upload
- `POST /v1/upload/presign` (returns presigned PUT and form data)
- `POST /v1/upload/complete` (finalize + trigger validation/transform)
- `GET /v1/upload/{objectId}/status`

#### Payment & Subscriptions
- `POST /v1/subscriptions` (create subscription)
- `GET /v1/subscriptions/current` (get active subscription)
- `PATCH /v1/subscriptions/plan` (upgrade/downgrade)
- `DELETE /v1/subscriptions/cancel` (cancel subscription)
- `POST /v1/payment-methods` (add payment method)
- `GET /v1/payment-methods` (list payment methods)
- `DELETE /v1/payment-methods/{id}` (remove payment method)
- `GET /v1/invoices` (list invoices)
- `GET /v1/invoices/{id}/download` (download PDF)
- `POST /v1/billing-portal` (generate Stripe portal session)
- `GET /v1/usage` (get current billing period usage)

#### Notifications
- `GET /v1/notifications` (list user notifications)
- `PATCH /v1/notifications/{id}/read` (mark as read)
- `DELETE /v1/notifications/{id}` (dismiss notification)
- `GET /v1/notification-preferences` (get preferences)
- `PUT /v1/notification-preferences` (update preferences)
- `POST /v1/webhooks` (create webhook endpoint)
- `GET /v1/webhooks` (list webhooks)
- `DELETE /v1/webhooks/{id}` (delete webhook)
- `POST /v1/webhooks/{id}/test` (test webhook delivery)

**Auth:** Bearer access token (JWT) for user sessions; static or bounded-scope keys for API clients.

### 3.2 gRPC Services (C++)

All gRPC services use **mTLS for service-to-service authentication** and validate **JWT tokens** from the BFF layer.

#### Authentication Flow
```
1. Client → FastAPI BFF (JWT Bearer token)
2. BFF validates JWT + checks Redis blacklist
3. BFF → gRPC service (JWT in metadata + mTLS)
4. gRPC service validates JWT + enforces authorization
```

#### auth.proto (excerpt)

```protobuf
service AuthService {
  rpc Login(LoginRequest) returns (LoginResponse);
  rpc ExchangeOtp(ExchangeOtpRequest) returns (TokenResponse);
  rpc SetupTotp(SetupTotpRequest) returns (SetupTotpResponse);
  rpc VerifyTotp(VerifyTotpRequest) returns (TokenResponse);
  rpc CreateApiKey(CreateApiKeyRequest) returns (CreateApiKeyResponse);
  rpc RevokeApiKey(RevokeApiKeyRequest) returns(google.protobuf.Empty);
}

// JWT Token Structure (returned in LoginResponse, TokenResponse)
message TokenResponse {
  string access_token = 1;   // JWT (RS256) with 15-minute expiry
  string refresh_token = 2;  // Opaque token with 30-day expiry
  int64 expires_in = 3;      // Seconds until access_token expires
  string token_type = 4;     // Always "Bearer"
}
```

#### upload.proto (excerpt)

```protobuf
service UploadService {
  rpc CreatePresignedUpload(PresignRequest) returns (PresignResponse);
  rpc CompleteUpload(CompleteUploadRequest) returns (UploadStatus);
  rpc GetUploadStatus(GetUploadStatusRequest) returns (UploadStatus);
}
```

#### payment.proto (excerpt)

```protobuf
service PaymentService {
  rpc CreateSubscription(CreateSubscriptionRequest) returns (Subscription);
  rpc UpdateSubscription(UpdateSubscriptionRequest) returns (Subscription);
  rpc CancelSubscription(CancelSubscriptionRequest) returns (Subscription);
  rpc AddPaymentMethod(AddPaymentMethodRequest) returns (PaymentMethod);
  rpc GetUsage(GetUsageRequest) returns (UsageReport);
  rpc CreateInvoice(CreateInvoiceRequest) returns (Invoice);
}
```

#### notification.proto (excerpt)

```protobuf
service NotificationService {
  rpc SendNotification(SendNotificationRequest) returns (NotificationResponse);
  rpc GetUserNotifications(GetUserNotificationsRequest) returns (NotificationList);
  rpc MarkAsRead(MarkAsReadRequest) returns (google.protobuf.Empty);
  rpc UpdatePreferences(UpdatePreferencesRequest) returns (NotificationPreferences);
  rpc CreateWebhook(CreateWebhookRequest) returns (Webhook);
  rpc DeliverWebhook(DeliverWebhookRequest) returns (WebhookDelivery);
}
```

### 3.3 UI (React/TS)

- Stateless forms with CSRF tokens, WebAuthn optional future extension
- Upload uses presigned URLs with multipart/resumable support (AWS S3 multipart)

---

## 4. Data Model

### 4.1 Core Entities

| Entity | Schema |
|--------|--------|
| **User** | `{ id, tenantId, email, emailVerified, passwordHash, totpEnabled, status }` |
| **Session** | `{ id, userId, deviceInfo, ipHash, refreshTokenHash, createdAt, expiresAt }` |
| **Role** | `{ id, tenantId, name, permissions[] }` |
| **ApiKey** | `{ id, tenantId, name, prefix, hash, scopes[], createdAt, expiresAt, status }` |
| **UploadObject** | `{ id, tenantId, ownerId, bucket, key, size, checksum, status, metadata, transforms[], retentionPolicy }` |
| **Quota** | `{ tenantId, limits{ dailyBytes, monthlyBytes, maxObjectSize, count }, usage{...} }` |
| **Subscription** | `{ id, tenantId, plan, status, currentPeriodStart, currentPeriodEnd, cancelAtPeriodEnd, trialEnd }` |
| **PaymentMethod** | `{ id, tenantId, type, last4, brand, expiryMonth, expiryYear, isDefault }` |
| **Invoice** | `{ id, tenantId, subscriptionId, amountDue, amountPaid, currency, status, dueDate, paidAt, items[] }` |
| **UsageRecord** | `{ id, tenantId, subscriptionId, metricId, quantity, timestamp, aggregationPeriod }` |
| **Notification** | `{ id, userId, type, channel, title, body, data, read, sentAt, readAt }` |
| **NotificationPreference** | `{ userId, channel, categories[], frequency, quietHours }` |
| **Webhook** | `{ id, tenantId, url, events[], secret, status, retryPolicy }` |
| **EmailTemplate** | `{ id, name, subject, bodyHtml, bodyText, variables[], locale }` |

> **Note:** All entities carry `tenantId` for strict isolation.

---

## 5. Functional Requirements (Verifiable "Shall")

### A. Authentication & Roles

#### A-1 Login & Tokens

1. The system shall support username/email + password login with rate limiting (per IP and per account).
2. The system shall issue JWT access tokens (RS256, ≤ 15 min expiry) and refresh tokens (≤ 30 days, rotate on use).
3. The system shall bind refresh tokens to session/device and shall allow remote session revocation via Redis blacklist.
4. The system shall sign JWTs with RS256 (RSA-SHA256) using 4096-bit keys stored in AWS KMS or Azure Key Vault.
5. The system shall include standard JWT claims: `iss`, `sub`, `aud`, `exp`, `iat`, `nbf`, `jti`, and custom claims: `tenant_id`, `email`, `roles`, `permissions`, `plan`.

#### A-2 2FA (TOTP) & OTP

6. The system shall support TOTP enrollment (QR provisioning, RFC 6238) and verification during login.
7. The system shall support out-of-band OTP delivery via email and SMS for step-up or recovery.
8. The system shall store TOTP secrets and backup codes encrypted at rest using a tenant-scoped KEK.

#### A-3 Credential & Profile Management

9. The system shall allow changing password with current-password check and breached-password screening.
10. The system shall implement a two-step email change (initiate → link/OTP confirm) and preserve old email until confirmation.
11. The system shall provide multi-session management (list, revoke, device nickname).

#### A-4 API Keys

12. The system shall allow tenant admins to create API keys with name, scope(s), expiry, and status (active/revoked).
13. The system shall store only a hash of API keys and display the secret exactly once on creation.
14. The system shall enforce scopes and tenant isolation for API key calls.

#### A-5 Roles & Authorization

15. The system shall implement RBAC with predefined roles: OWNER, ADMIN, DEVELOPER, ANALYST, VIEWER.
16. The system shall support custom roles with permission sets (CRUD on resources, admin actions).
17. The system shall support ABAC conditions (tenant plan, region, resource tags) for fine-grained policies.
18. The system shall evaluate authorization server-side on each request; clients shall not rely on UI checks.

#### A-6 Auditing

19. The system shall record audit logs for login, logout, failed logins, 2FA events, API key lifecycle, role changes, token revocation.

#### A-7 Token Security & Lifecycle

20. The system shall detect refresh token reuse and revoke the corresponding session and any tokens minted after the compromised point.
21. The system shall support token rotation with grace periods and blacklist revoked tokens in Redis until expiry.
22. The system shall validate JWT tokens on every request by checking: signature validity, expiration, audience, issuer, and Redis blacklist status.
23. The system shall support JWT key rotation with a grace period (24 hours) where both old and new public keys are accepted.

#### A-8 Service-to-Service Authentication

24. The system shall use mTLS (Mutual TLS) for all internal gRPC service-to-service communication.
25. The system shall validate client certificates on the server side and server certificates on the client side.
26. The system shall rotate service certificates automatically every 90 days using cert-manager (Kubernetes) or equivalent.
27. The system shall use TLS 1.3 where supported, with fallback to TLS 1.2; TLS 1.0 and 1.1 shall be disabled.

#### A-9 Rate Limiting & Account Protection

28. The system shall implement rate limits per identity type: user, tenant, API key, IP; include `X-RateLimit-Limit`, `X-RateLimit-Remaining`, and `X-RateLimit-Reset` headers.
29. When 20 failed logins occur for the same account within 2 minutes, the system shall lock the account for 15 minutes and emit `AUDIT_AUTH_RATE_LIMIT` within 200 ms.

#### A-10 Idempotency

30. The system shall treat requests with identical `Idempotency-Key` within a 24-hour window as the same operation and return the original result without re-executing side effects.

---

### B. Upload

#### B-1 Presigned Upload

23. The system shall return a presigned URL (or form fields) for client-side upload to S3/R2 with content-type, max size, checksum constraints.
24. The system shall support multipart/resumable uploads and report part ETags.
25. The system shall verify client-provided checksum (e.g., SHA-256) and compare with S3 ETag or server-computed hash.
26. The system shall issue presigned URLs with TTL ≤ 10 minutes, bound to `Content-Length`, `Content-Type`, and a canonical object key; any drift shall be rejected by the storage service.

#### B-2 Security & Validation

27. The system shall enforce tenant/user quotas (bytes/day, bytes/month, object count, max object size).
28. The system shall perform antivirus scanning (e.g., ClamAV/Lambda layer) before marking an object as CLEAN.
29. The system shall validate MIME type, extension policy, and max dimensions/duration for images/video.
30. The system shall reject and purge objects that fail validation, emitting an audit/event with reason code.
31. The system shall detect archive bombs by enforcing `max_uncompressed_size`, `max_file_count`, and tree depth; offending uploads shall be rejected with `UP_002`.
32. The system shall store checksums (client-supplied SHA-256 + server-side verification); reject if mismatch with `UP_005`.
33. The system shall use deterministic path layout (`tenantId/YYYY/MM/{objectId}`) to prevent key guessing; no user-controlled path segments allowed.

#### B-3 Transformation Pipeline

34. The system shall support post-upload transformations (e.g., image resize, PDF OCR, video transcode) via:
    - **AWS:** S3 event → Lambda/SQS/Step Functions
    - **Azure:** Blob event → Azure Functions/Queue Storage/Durable Functions

35. The system shall store transform artifacts under deterministic keys (e.g., `/derived/{objectId}/{profile}.ext`).
36. The system shall expose idempotent reprocessing (manual retrigger) with previous artifacts preserved or versioned.
37. The system shall support declarative transformation profiles with `profileId`, `type` (image/video/pdf), `steps[]`, and `limits`.

#### B-4 Encryption & Retention

38. The system shall enable server-side encryption with SSE-KMS and support per-tenant CMKs.
39. The system shall allow per-object retention policies (legal hold, TTL) mapped to bucket lifecycle rules.

#### B-5 Observability & Status

40. The system shall track upload state machine: `REQUESTED → UPLOADING → VALIDATING → TRANSFORMING → CLEAN → AVAILABLE | REJECTED | FAILED`.
41. The system shall publish events (upload completed, validation failed, transform completed) for notifications/webhooks.
42. The system shall provide `GET /v1/upload/{objectId}/status` with current state, errors, and artifact URIs (signed).

#### B-6 Pagination & List Operations

43. The system shall implement cursor-based pagination with `nextCursor` for all list operations; hard cap at 1000 items per request.
44. The system shall support filtering and sorting on upload lists by status, date, size, and owner.

---

### C. Payment & Subscriptions

#### C-1 Subscription Lifecycle

45. The system shall support subscription states: `trialing`, `active`, `past_due`, `canceled`, `unpaid`, `paused`.
46. The system shall automatically transition subscriptions based on payment status and support grace periods (3 days for failed payments).
47. The system shall support trial periods (7, 14, or 30 days) with automatic conversion to paid upon trial end.
48. The system shall allow immediate cancellation or cancellation at period end with pro-rata refunds (configurable).

#### C-2 Payment Processing

49. The system shall integrate with Stripe for payment processing with PCI-DSS Level 1 compliance (no card storage).
50. The system shall support payment methods: credit/debit cards, ACH, SEPA, Apple Pay, Google Pay.
51. The system shall implement Strong Customer Authentication (SCA/3D Secure 2.0) for EU compliance.
52. The system shall tokenize payment methods and store only tokens, never raw card data.
53. The system shall retry failed payments automatically using exponential backoff (1, 3, 7 days) with email notifications.

#### C-3 Plans & Pricing

54. The system shall support predefined plans: **Free** (0/mo), **Pro** ($29/mo), **Enterprise** (custom).
55. The system shall support both flat-fee and usage-based billing components (metered storage, API calls, transforms).
56. The system shall calculate proration on plan upgrades/downgrades based on time remaining in billing period.
57. The system shall support annual billing with discounts (e.g., 2 months free for annual commitment).

#### C-4 Invoicing & Billing

58. The system shall generate invoices automatically at billing cycle start (monthly or annual).
59. The system shall include line items: subscription fees, usage charges, credits, taxes, and discounts.
60. The system shall calculate taxes using Stripe Tax or Avalara based on customer location.
61. The system shall provide invoice download as PDF with company branding and support multi-currency (USD, EUR, GBP).
62. The system shall send invoice emails 3 days before due date and payment receipt emails immediately after successful payment.

#### C-5 Usage Tracking & Metering

63. The system shall meter usage for: storage (GB-hours), API calls, upload bandwidth, transform minutes.
64. The system shall aggregate usage records hourly and calculate billing amounts daily.
65. The system shall expose real-time usage API endpoint showing current period consumption vs. plan limits.
66. The system shall send usage alerts at 80% and 100% of quota thresholds.

#### C-6 Payment Security & Compliance

67. The system shall log all payment events (successful, failed, refunded) with correlation IDs for audit.
68. The system shall implement fraud detection using Stripe Radar with custom rules for high-risk transactions.
69. The system shall support payment disputes and chargebacks with automated workflow notifications.
70. The system shall maintain PCI-DSS compliance by never logging or storing sensitive payment data.

---

### D. Notifications & Communication

#### D-1 Notification Channels

71. The system shall support notification delivery via: email, SMS, push notifications (FCM/APNs), in-app, and webhooks.
72. The system shall allow users to configure notification preferences per category (security, billing, uploads, system).
73. The system shall support quiet hours (do not send non-critical notifications during 10 PM - 8 AM user local time).
74. The system shall batch non-urgent notifications and send as daily digest (configurable).

#### D-2 Notification Types

75. The system shall send notifications for:
    - **Security:** Login from new device, password changed, 2FA enabled/disabled, suspicious activity
    - **Billing:** Payment successful, payment failed, trial ending, subscription canceled, invoice available
    - **Upload:** Upload completed, validation failed, transform completed, quota warning
    - **System:** Maintenance scheduled, service degradation, feature announcements

#### D-3 Template Management

76. The system shall use template engine (Handlebars/Liquid) for email/SMS bodies with variable interpolation.
77. The system shall version templates and support A/B testing of email subject lines and content.
78. The system shall localize templates based on user language preference (en, de, fr) with fallback to English.
79. The system shall validate template variables before send to prevent missing data errors.

#### D-4 Delivery & Tracking

80. The system shall queue notifications in Redis/SQS with retry logic (max 3 retries with exponential backoff).
81. The system shall track delivery status: `queued`, `sent`, `delivered`, `bounced`, `failed`, `read` (for email opens).
82. The system shall implement email deliverability best practices: SPF, DKIM, DMARC, unsubscribe links, List-Unsubscribe header.
83. The system shall respect user opt-outs and unsubscribe preferences (except critical security notifications).

#### D-5 Webhooks

84. The system shall allow tenants to register webhook endpoints for events: `user.created`, `subscription.updated`, `upload.completed`, etc.
85. The system shall sign webhook payloads using HMAC-SHA256 with tenant-specific secret for verification.
86. The system shall retry failed webhook deliveries up to 5 times with exponential backoff (1s, 5s, 30s, 5min, 30min).
87. The system shall provide webhook logs with request/response payloads (truncated to 10KB) for debugging.
88. The system shall disable webhooks after 10 consecutive failures and notify tenant admin.

#### D-6 In-App Notifications

89. The system shall maintain in-app notification feed with unread count and mark-as-read functionality.
90. The system shall expire in-app notifications after 30 days and archive after 90 days.
91. The system shall support real-time notification delivery via WebSocket with fallback to polling (30s interval).

---

### E. Email & Transaction Management

#### E-1 Transactional Email System

92. The system shall integrate with SendGrid/AWS SES/Postmark for transactional email delivery.
93. The system shall support email types: verification, password reset, OTP, invoices, receipts, notifications, marketing (opt-in).
94. The system shall enforce rate limits per email type to prevent abuse: verification (5/hour), password reset (3/hour).
95. The system shall track email metrics: sent, delivered, opened, clicked, bounced, complained (spam).

#### E-2 Email Queue & Delivery

96. The system shall queue emails in PostgreSQL with `pending`, `processing`, `sent`, `failed` states.
97. The system shall process email queue asynchronously with worker pools (min 2, max 10 workers) scaled by queue depth.
98. The system shall implement priority queues: `critical` (OTP, password reset), `high` (transactional), `normal` (notifications), `low` (marketing).
99. The system shall dequeue and send emails within 30 seconds for critical, 5 minutes for high, 30 minutes for normal.

#### E-3 Email Templates & Personalization

100. The system shall store templates with subject, HTML body, text body (for plain-text clients), and preview text.
101. The system shall support dynamic content insertion: user name, company name, action buttons, one-time links.
102. The system shall automatically inline CSS for maximum email client compatibility.
103. The system shall provide template preview and test send functionality for administrators.

#### E-4 Distributed Transaction Management

104. The system shall use database transactions with proper isolation levels: `READ COMMITTED` (default), `SERIALIZABLE` (financial operations).
105. The system shall implement saga pattern for distributed transactions spanning multiple services (payment + subscription + email).
106. The system shall use idempotency keys for all state-changing operations to ensure exactly-once semantics.
107. The system shall implement compensating transactions for rollback scenarios (e.g., refund on failed subscription activation).

#### E-5 Transaction Monitoring

108. The system shall log all database transactions with duration, isolation level, and affected tables.
109. The system shall alert on long-running transactions (> 5 seconds) and deadlocks.
110. The system shall maintain transaction audit trail for financial operations with immutable records.

---

## 6. Non-Functional Requirements (NFRs)

### 6.1 Performance (SLOs & Quality Attribute Scenarios)

**NFR-P1: Login Performance**
- **Scenario:** User submits valid credentials during normal operations.
- **Response:** 95th percentile login round-trip ≤ 500 ms (no 2FA) and ≤ 1.2 s (with TOTP).
- **Measurement:** Prometheus histogram `auth_login_duration_seconds`.

**NFR-P1a: JWT Validation Performance**
- **Scenario:** API request with JWT Bearer token during normal operations.
- **Response:** 95th percentile JWT validation (signature + blacklist check) ≤ 2 ms.
- **Measurement:** Prometheus histogram `auth_jwt_validation_duration_seconds`.

**NFR-P1b: mTLS Handshake Performance**
- **Scenario:** Internal gRPC service-to-service call establishment.
- **Response:** Full TLS handshake ≤ 15 ms; TLS resume with session tickets ≤ 3 ms.
- **Measurement:** Prometheus histogram `grpc_mtls_handshake_duration_seconds`.

**NFR-P2: Presign Performance**
- **Scenario:** 300 presign requests/sec (burst) from 50 tenants during normal operations.
- **Response:** 99% presign latency ≤ 300 ms; error rate < 0.1%.
- **Measurement:** Prometheus histogram `upload_presign_latency_seconds`.

**NFR-P3: Upload Status Visibility**
- **Response:** Upload status update visible ≤ 2 s after S3 completion.

**NFR-P4: Payment Processing Performance**
- **Scenario:** User submits payment during checkout.
- **Response:** 95th percentile payment confirmation ≤ 3 s (including Stripe API).
- **Measurement:** Prometheus histogram `payment_processing_duration_seconds`.

**NFR-P5: Notification Delivery Performance**
- **Scenario:** System triggers notification during normal operations.
- **Response:** In-app notifications delivered ≤ 1 s; emails queued ≤ 500 ms; critical emails sent ≤ 30 s.
- **Measurement:** Prometheus histograms `notification_queue_duration_seconds`, `email_send_duration_seconds`.

**NFR-P6: Email Queue Processing**
- **Response:** Email queue depth should not exceed 1000 messages; process rate ≥ 100 emails/minute.

### 6.2 Availability & Reliability

- **NFR-A1:** Auth endpoints shall meet 99.9% monthly SLO; breach triggers incident severity S2.
- **NFR-A2:** Upload pipeline degradations must not block existing downloads.
- **NFR-A3:** Transform queue processing guarantees at-least-once with idempotent handlers.
- **NFR-A4:** Payment endpoints shall meet 99.95% SLO; failed payments shall retry automatically.
- **NFR-A5:** Critical notification delivery (security, payment) shall have 99.9% success rate.
- **NFR-A6:** Email delivery shall achieve 98% deliverability rate (excluding bounces/spam).

### 6.3 Security & Privacy

- **NFR-S1:** Access tokens are JWT (RS256) with 4096-bit RSA keys and asymmetric signing (kid rotation supported); refresh tokens are opaque and stored hashed with Argon2id.
- **NFR-S2:** Zero trust between BFF and clients; mTLS (mutual TLS with certificate validation) required between all internal gRPC services.
- **NFR-S3:** JWT signing keys and service certificates managed in AWS KMS/Azure Key Vault; no plaintext secrets in code or images.
- **NFR-S4:** PII minimized in logs; IPs/device fingerprints stored as salted hashes; JWT tokens logged with only last 8 characters.

**NFR-S5: Rate Limit Enforcement**
- **Scenario:** When 20 failed logins occur for the same account within 2 minutes.
- **Response:** The system shall lock the account for 15 minutes and emit `AUDIT_AUTH_RATE_LIMIT` within 200 ms.

**NFR-S5a: Token Revocation**
- **Scenario:** User logs out or security event triggers token revocation.
- **Response:** Token blacklisted in Redis within 100 ms; subsequent API calls with revoked token rejected within 2 ms.

**NFR-S5b: mTLS Certificate Validation**
- All internal gRPC services shall verify client and server certificates using configured CA.
- Certificate expiry shall be monitored; alerts triggered 7 days before expiration.
- Invalid or expired certificates shall cause immediate connection rejection.

**NFR-S6: Payment Data Security**
- The system shall never log, store, or transmit raw payment card data; use tokenization only.
- All payment operations shall be logged with PCI-DSS compliant audit trails (card last 4 digits only).

**NFR-S7: Webhook Security**
- All webhook payloads shall be signed with HMAC-SHA256.
- Webhook URLs shall be validated to prevent SSRF attacks (no private IPs, localhost).

### 6.4 Compliance

- **NFR-C1:** Meet GDPR principles (lawful basis, data minimization, erasure).
- **NFR-C2:** Support data export (user sessions, keys, uploads metadata) in machine-readable form.
- **NFR-C3:** Data retention periods shall be documented and enforced per entity type with automated lifecycle policies.
- **NFR-C4:** Payment processing shall be PCI-DSS Level 1 compliant via certified payment gateway (Stripe).
- **NFR-C5:** Email communications shall comply with CAN-SPAM Act: include unsubscribe link, physical address, honor opt-outs within 10 days.
- **NFR-C6:** Strong Customer Authentication (SCA) shall be implemented for EU customers per PSD2 requirements.

### 6.5 Internationalization & Accessibility

- **NFR-I1:** UI supports i18n (en, de, fr baseline) for auth/upload flows; email/SMS templates localized with fallback to English.
- **NFR-I2:** Auth screens shall meet WCAG 2.1 AA standards; labeled inputs for 2FA; error hints localized.
- **NFR-I3:** RTL (right-to-left) language support readiness for future internationalization.

### 6.6 Disaster Recovery & Business Continuity

- **NFR-DR1:** RPO (Recovery Point Objective) ≤ 15 minutes via bucket versioning + DB WAL shipping.
- **NFR-DR2:** RTO (Recovery Time Objective) ≤ 1 hour for critical auth and upload services.
- **NFR-DR3:** The system shall support point-in-time recovery of identity and audit tables within 15 minutes granularity for the last 7 days.
- **NFR-DR4:** Chaos testing shall validate resilience against S3 read outage, KMS throttling, and AV engine failure scenarios.

---

## 7. Constraints & Assumptions

- **Storage:** AWS S3 or Cloudflare R2; interchangeable via S3 API.
- **Compute:** Transform functions limited to provider's runtime quotas (timeout, memory).
- **Networking:** All public endpoints via HTTPS; HSTS enabled; TLS 1.2+.
- **DB:** PostgreSQL/TimescaleDB for identity/audit/quotas; Redis for session throttling/rate limits.

---

## 8. Authorization Model (Detail)

### 8.1 Permission Grammar

Permissions follow the format: `resource:action:scope`

**Examples:**
- `user:read`, `user:update`, `session:revoke`
- `apikey:create`, `upload:presign`, `upload:read`, `upload:delete`
- `role:assign`
- `subscription:read`, `subscription:update`, `payment-method:create`
- `notification:send`, `webhook:create`, `webhook:test`

### 8.2 Scope Semantics (API keys)
- `read:*` — Read-only access to all resources within tenant
- `write:upload` — Upload and modify upload objects
- `admin:tenant` — Full tenant administration
- `billing:read` — View subscription and invoices
- `billing:write` — Manage subscriptions and payment methods
- `notification:send` — Send notifications and manage webhooks

### 8.3 ABAC Policy Examples
- Deny `upload:presign` when `tenant.plan == free AND requestedSize > 50MB`
- Allow `upload:delete` only if `resource.ownerId == user.id OR user.role IN [ADMIN, OWNER]`
- Deny `subscription:downgrade` when `usage.current > newPlan.limits`
- Allow `webhook:create` only if `tenant.plan IN [pro, enterprise]`

### 8.4 Policy Evaluation & Conflict Resolution

- **Evaluation Order:** The system shall evaluate ABAC conditions before RBAC overrides.
- **Conflict Resolution:** In conflicts, deny applies (deny-overrides strategy).
- **Policy Versioning:** Policies are versioned; breaking changes require migration scripts and backward compatibility period.

---

## 9. Error Handling & Codes

| Code | Description |
|------|-------------|
| **AUTH_001** | Invalid credentials |
| **AUTH_002** | 2FA required / TOTP needed |
| **AUTH_003** | OTP expired/invalid |
| **AUTH_004** | Token revoked |
| **AUTH_005** | Password policy violation |
| **AUTH_006** | Email change token invalid |
| **UP_001** | Quota exceeded |
| **UP_002** | Invalid MIME type |
| **UP_003** | Antivirus failed (infected) |
| **UP_004** | Transformation failed (detail code) |
| **UP_005** | Checksum mismatch |
| **UP_006** | Object not found / not owned |
| **PAY_001** | Payment method declined |
| **PAY_002** | Insufficient funds |
| **PAY_003** | Card expired |
| **PAY_004** | Payment processor error |
| **PAY_005** | Subscription not found |
| **PAY_006** | Invalid plan transition |
| **PAY_007** | Invoice already paid |
| **PAY_008** | Refund failed |
| **NOTIF_001** | Invalid notification channel |
| **NOTIF_002** | Template not found |
| **NOTIF_003** | Template rendering error |
| **NOTIF_004** | Delivery failed (max retries) |
| **NOTIF_005** | Webhook signature invalid |
| **NOTIF_006** | Webhook URL unreachable |
| **EMAIL_001** | Email send failed |
| **EMAIL_002** | Email bounced (hard bounce) |
| **EMAIL_003** | Spam complaint |
| **EMAIL_004** | Rate limit exceeded |

**Error Response Format:**
```json
{
  "code": "ERROR_CODE",
  "message": "Human-readable message",
  "correlationId": "uuid",
  "details": {}
}
```

---

## 10. Threat Model & Security Controls

### 10.1 STRIDE Analysis

| Threat Category | Attack Vector | Mitigation | Control ID |
|----------------|---------------|------------|------------|
| **Spoofing** | Credential stuffing | Rate limiting, breached password check, account lockout | NFR-S5, SEC-01 |
| **Tampering** | Token manipulation | Asymmetric JWT signing, token integrity validation | NFR-S1 |
| **Repudiation** | Deny malicious actions | Immutable audit logs with correlation IDs | SEC-02 |
| **Information Disclosure** | Token theft via XSS | HttpOnly cookies, CSP headers, short token TTL | SEC-03 |
| **Information Disclosure** | Presigned URL leakage | Bounded TTL (≤10 min), content-type binding | B-26 |
| **Denial of Service** | Upload zip bombs | Size limits, uncompressed size checks, depth limits | B-31 |
| **Denial of Service** | Rate limit bypass | Per-IP, per-account, per-tenant limits with Redis | A-20 |
| **Elevation of Privilege** | TOCTOU on presigned URLs | Checksum validation, content-type enforcement | B-25, B-26 |
| **Tampering** | Payment amount manipulation | Server-side price calculation, signed payment intents | C-49 |
| **Information Disclosure** | Payment card theft | Tokenization, no card storage, PCI-DSS compliance | C-52, NFR-S6 |
| **Spoofing** | Webhook forgery | HMAC-SHA256 signature verification | D-85, NFR-S7 |
| **Denial of Service** | Webhook SSRF attack | URL validation, no private IPs/localhost | NFR-S7 |
| **Information Disclosure** | Email interception | TLS for SMTP, DKIM signing | E-82 |

### 10.2 Upload Security Controls

| Attack | Control | Requirement |
|--------|---------|-------------|
| **Path Traversal** | Deterministic key layout, no user path segments | B-33 |
| **Polyglot Files** | MIME type validation, extension sniffing, deep content inspection | B-29 |
| **Malware** | AV scanning (ClamAV) + optional sandbox | B-28 |
| **Zip Bombs** | Max uncompressed size, file count, tree depth limits | B-31 |
| **Large File DoS** | Quota enforcement, max object size | B-27 |
| **Checksum Bypass** | Client + server-side SHA-256 verification | B-32 |

### 10.3 Security Controls Summary

| Control | Implementation |
|---------|----------------|
| **Password** | Argon2id with calibrated memory cost; minimum 12 chars; breach check via k-Anon |
| **Tokens** | JWT with aud, iss, exp, nbf, jti; refresh rotation + reuse-detection |
| **Transport** | HSTS, SameSite=strict cookies (if cookie mode), CSRF tokens on state-changing POSTs |
| **Storage** | SSE-KMS; per-tenant CMK optional |
| **Rate Limits** | Login 5/min/account, OTP 3/min, presign 60/min/user, API keys per scope configurable |
| **Presigned URLs** | TTL ≤ 10 min, bound to Content-Length, Content-Type, canonical key |
| **Quarantine** | Failed uploads moved to quarantine bucket with 7-day retention |

---

## 11. Monitoring, Observability & Alerts

### 11.1 Key Metrics (Prometheus/Grafana)

| Metric | Type | Purpose | Alert Threshold |
|--------|------|---------|-----------------|
| `auth_login_success_total` | Counter | Track successful logins | N/A |
| `auth_login_failure_total` | Counter | Track failed logins | > 100/min → SEV-3 |
| `auth_login_duration_seconds` | Histogram | Login latency | p95 > 500ms for 5min → SEV-3 |
| `auth_totp_verify_total` | Counter | 2FA step-ups | N/A |
| `auth_account_locked_total` | Counter | Rate limit triggers | > 50/hour → SEV-2 |
| `apikey_usage_total` | Counter (by scope) | API key activity | Anomaly detection |
| `upload_presign_latency_seconds` | Histogram | Presign performance | p95 > 300ms for 5min → SEV-3 |
| `upload_size_bytes` | Histogram | Upload size distribution | N/A |
| `upload_validation_failed_total` | Counter (by reason) | Validation failures | > 10% failure rate → SEV-2 |
| `upload_av_detected_total` | Counter | AV detections | Any detection → SEV-1 |
| `transform_duration_seconds` | Histogram | Transform latency | N/A |
| `transform_backlog_age_seconds` | Gauge | Queue lag | > 300s for 10min → SEV-2 |
| `payment_success_total` | Counter | Successful payments | N/A |
| `payment_failed_total` | Counter (by reason) | Failed payments | > 5% failure rate → SEV-2 |
| `payment_processing_duration_seconds` | Histogram | Payment latency | p95 > 3s for 5min → SEV-3 |
| `subscription_churn_total` | Counter | Subscription cancellations | Anomaly detection |
| `mrr_amount` | Gauge | Monthly recurring revenue | N/A |
| `notification_sent_total` | Counter (by channel, type) | Notifications sent | N/A |
| `notification_failed_total` | Counter (by channel, reason) | Failed notifications | > 1% failure rate → SEV-3 |
| `notification_delivery_duration_seconds` | Histogram | Notification latency | p95 > 5s → SEV-3 |
| `webhook_delivery_success_total` | Counter | Successful webhooks | N/A |
| `webhook_delivery_failed_total` | Counter (by reason) | Failed webhooks | > 10% failure rate → SEV-2 |
| `email_sent_total` | Counter | Emails sent | N/A |
| `email_bounced_total` | Counter (hard/soft) | Email bounces | Hard bounce rate > 5% → SEV-2 |
| `email_queue_depth` | Gauge | Queue size | > 1000 for 10min → SEV-2 |
| `auth_jwt_validation_duration_seconds` | Histogram | JWT signature verification + blacklist check | p95 > 5ms → alert |
| `auth_jwt_validation_errors_total` | Counter | JWT validation failures by error type (expired, invalid, revoked) | rate > 10/min → alert |
| `auth_blacklist_check_duration_seconds` | Histogram | Redis blacklist lookup latency | p95 > 3ms → alert |
| `auth_refresh_token_rotations_total` | Counter | Successful refresh token rotations | - |
| `auth_refresh_token_reuse_detected_total` | Counter | Token reuse attempts (security breach) | > 0 → critical alert |
| `auth_mtls_handshake_duration_seconds` | Histogram | mTLS handshake time | p95 > 20ms → alert |
| `auth_mtls_handshake_errors_total` | Counter | mTLS handshake failures by error type | rate > 1/min → alert |
| `redis_blacklist_keys_count` | Gauge | Number of blacklisted tokens in Redis | Monitor for memory |
| `redis_blacklist_hit_rate` | Gauge | Cache hit rate for blacklist checks | < 95% → investigate |
| `cert_expiry_timestamp_seconds` | Gauge | mTLS certificate expiry time | < 7 days → warning alert |

### 11.2 Structured Logging

- **Format:** JSON with `tenantId`, `userId`, `sessionId`, `correlationId`, `timestamp`, `level`, `message`
- **PII Handling:** IPs/device fingerprints stored as salted hashes
- **Retention:** 90 days hot, 1 year cold storage (S3/Glacier)

### 11.3 Audit Trails

- **Storage:** Immutable append-only stream (Postgres table + S3 archive)
- **Events:** Login, logout, failed logins, 2FA events, API key lifecycle, role changes, upload lifecycle, quota breaches, subscription changes, payment events, refunds, notification sends, webhook deliveries
- **Retention:** 7 years for compliance (financial events), 3 years for operational events

### 11.4 Distributed Tracing

- **Tool:** OpenTelemetry with Jaeger/Tempo
- **Spans:** BFF → gRPC, gRPC → DB, S3 operations, transform pipeline, payment gateway calls, email service calls, webhook deliveries
- **Sampling:** 1% baseline, 100% for errors, 100% for payment operations

---

## 12. Acceptance Criteria & Test Coverage

### 12.1 Authentication Acceptance Criteria

- **AC-A1:** Creating an API key yields a one-time secret, subsequent retrieval returns only metadata.
- **AC-A2:** Revoking a session invalidates its refresh token; access token expires naturally, further refresh attempts fail with `AUTH_004`.
- **AC-A3:** Using a revoked API key returns `401` with `AUTH_004`.
- **AC-A4:** After 20 failed login attempts within 2 minutes, account is locked for 15 minutes and audit event `AUDIT_AUTH_RATE_LIMIT` is emitted.
- **AC-A5:** Refresh token reuse is detected and entire session chain is revoked within 200ms.
- **AC-A6:** JWT validation (signature + blacklist check) completes in < 2ms at p95.
- **AC-A7:** Logout adds token to Redis blacklist within 100ms; subsequent API calls with same token return `401` with `AUTH_004`.
- **AC-A8:** Internal gRPC service calls use mTLS; invalid or expired certificate results in connection rejection.
- **AC-A9:** JWT key rotation with grace period: services accept both old (kid: v1) and new (kid: v2) keys for 24 hours.
- **AC-A10:** Token with invalid signature, expired timestamp, or wrong audience/issuer returns `401` with specific error message.

### 12.2 Upload Acceptance Criteria

- **AC-U1:** Uploading a 25 MB PNG with correct checksum moves to CLEAN → AVAILABLE within 5 s median after CompleteUpload.
- **AC-U2:** Uploading a disallowed executable is rejected with `UP_002` and object deleted from storage within 60s.
- **AC-U3:** Polyglot file (JPG+JS) is rejected with `UP_002`; object purged within 60s; audit reason `mime_mismatch` recorded.
- **AC-U4:** Zip bomb (1KB compressed → 1GB uncompressed) is rejected with `UP_002` during validation.
- **AC-U5:** Checksum mismatch results in `UP_005` error and object rejection.
- **AC-U6:** Presigned URL expires after 10 minutes and returns `403` from S3.

### 12.3 Payment & Subscription Acceptance Criteria

- **AC-P1:** Creating a subscription with valid payment method transitions to `active` state within 3 seconds.
- **AC-P2:** Failed payment moves subscription to `past_due` and triggers retry after 1 day with email notification.
- **AC-P3:** Upgrading from Pro to Enterprise applies pro-rata credit and updates subscription immediately.
- **AC-P4:** Canceling subscription with `cancel_at_period_end=true` maintains access until period end.
- **AC-P5:** Invoice is generated 3 days before billing date and sent via email with PDF attachment.
- **AC-P6:** Usage metering aggregates hourly and reflects in current period usage API within 5 minutes.
- **AC-P7:** Payment with 3D Secure required redirects to authentication page and completes after verification.
- **AC-P8:** Refund request processes within 30 seconds and updates invoice status to `refunded`.

### 12.4 Notification Acceptance Criteria

- **AC-N1:** Critical notification (password changed) delivers via all enabled channels within 30 seconds.
- **AC-N2:** User updating notification preferences takes effect immediately (within 1 second).
- **AC-N3:** Email with unsubscribe link honors opt-out and stops non-critical emails within 10 days.
- **AC-N4:** Webhook delivery retries 5 times with exponential backoff (1s, 5s, 30s, 5min, 30min).
- **AC-N5:** Invalid webhook signature returns `401` with `NOTIF_005` error.
- **AC-N6:** In-app notification appears in real-time (< 1s) via WebSocket and shows unread count.
- **AC-N7:** Quiet hours (10 PM - 8 AM) suppress non-critical notifications; delivered next morning.
- **AC-N8:** Template with missing variable fails validation and returns `NOTIF_003` before send.

### 12.5 Email & Transaction Acceptance Criteria

- **AC-E1:** Critical email (OTP) dequeues and sends within 30 seconds of queuing.
- **AC-E2:** Email template renders with personalized content (user name, action button) correctly.
- **AC-E3:** Hard bounce marks email as `bounced` and suppresses future sends to that address.
- **AC-E4:** Distributed transaction (subscription + payment + email) rolls back compensation if payment fails.
- **AC-E5:** Idempotency key on duplicate request returns cached result without re-executing.
- **AC-E6:** Long-running transaction (> 5 seconds) triggers alert and logs transaction details.

### 12.6 Testing Strategy Matrix

| Test Type | Coverage | Tools | Frequency |
|-----------|----------|-------|-----------|
| **Unit** | Policy evaluation, token generation, crypto functions, payment calculations | pytest, gtest | Every commit |
| **Integration** | S3 presign/complete, gRPC → DB, AV scanning, Stripe API, email delivery | pytest, testcontainers | Every PR |
| **E2E** | Browser + BFF flows, multi-step auth, upload lifecycle, checkout flow | Playwright, Cypress | Nightly |
| **Chaos** | S3 outage, KMS throttling, AV failure, DB failover, Stripe downtime | Chaos Toolkit, LitmusChaos | Weekly |
| **Security** | SAST, DAST, dependency scan, payment security testing | Semgrep, ZAP, Snyk | Every PR + Weekly |
| **Performance** | Login surge (1000 req/s), multi-part 5GB upload, payment load (500 req/s) | k6, Locust | Pre-release |
| **Accessibility** | WCAG 2.1 AA compliance | axe-core, Pa11y | Per UI change |
| **Payment** | Stripe test mode, failed payments, 3DS, refunds, webhooks | Stripe CLI, pytest | Every payment change |

### 12.7 Test Traceability

Every "shall" requirement must map to at least one automated test case. See RTM (Section 16) for full traceability.

---

## 13. Data Protection & Retention (GDPR-Aware)

### 13.1 Data Retention Policies

| Entity | Purpose | Legal Basis | Retention (Hot) | Retention (Archive) | Erasure Path | Notes |
|--------|---------|-------------|-----------------|---------------------|--------------|-------|
| **User** | Authentication | Contract | Account lifetime | N/A | DELETE cascade | Email anonymized on request |
| **Session** | Security | Legitimate interest | 30 days | N/A | Auto-expire | Revoked sessions purged after 7 days |
| **API Key** | Access control | Contract | Until revoked | N/A | Soft delete → purge 90d | Hash only stored |
| **Audit Log** | Compliance | Legal obligation | 90 days | 7 years | No erasure | PII pseudonymized |
| **Upload Object** | Service delivery | Contract | Per retention policy | Per policy | Soft delete → purge | Legal hold supported |
| **Upload Metadata** | Audit | Legitimate interest | 1 year | 7 years | On request | Owner ID retained |
| **Subscription** | Billing | Contract | Account lifetime + 7 years | N/A | Legal obligation (tax) | Required for accounting |
| **Payment Method** | Payment processing | Contract | Until removed | N/A | Immediate (token only) | Token deleted in Stripe |
| **Invoice** | Financial records | Legal obligation | 7 years | N/A | No erasure | Tax compliance |
| **Payment Event** | Audit | Legal obligation | 7 years | N/A | No erasure | Fraud prevention |
| **Notification** | Service delivery | Legitimate interest | 90 days | N/A | Auto-purge | Unread moved to archive |
| **Email Log** | Delivery tracking | Legitimate interest | 90 days | 1 year | Auto-purge | Bounce data retained |
| **Webhook Log** | Debugging | Legitimate interest | 30 days | N/A | Auto-purge | PII in payload anonymized |

### 13.2 Right to Erasure (GDPR Article 17)

- **User-initiated:** Self-service export + delete via UI.
- **Admin-initiated:** Tenant admin can trigger per-user erasure.
- **Process:** 
  1. Mark user for deletion
  2. Anonymize PII in logs/audit (replace with pseudonym)
  3. Delete identity records
  4. Soft-delete owned uploads (or reassign to tenant)
  5. Generate erasure certificate with correlation ID

### 13.3 Right to Data Portability (GDPR Article 20)

- **Format:** JSON export of user profile, sessions, API keys, upload metadata, subscription history, invoices, notification preferences.
- **Endpoint:** `GET /v1/users/me/export` (async, returns signed download URL).
- **Scope:** User's own data within tenant.
- **Financial Data:** Invoices included as PDF attachments in export archive.

### 13.4 Encryption & Key Management

- **At Rest:** SSE-KMS for S3; AES-256-GCM for DB fields (TOTP secrets, backup codes).
- **In Transit:** TLS 1.2+ (prefer 1.3); mutual TLS for gRPC.
- **Key Hierarchy:** Master key (KMS) → Tenant KEK → Data encryption keys.
- **Rotation:** Tenant KEKs rotated annually; master key rotation per org policy.

---

## 14. API Versioning & Compatibility

### 14.1 Versioning Strategy

- **URL Versioning:** `/v1/...`, `/v2/...` for major versions.
- **Semantic Versioning:** MAJOR.MINOR.PATCH for gRPC services.
- **Deprecation Policy:** 6-month overlap for breaking changes; `Sunset` header included.

### 14.2 Breaking Changes

- **Definition:** Removing fields, changing types, new required parameters, auth changes.
- **Process:** 
  1. Announce via changelog and email 3 months prior
  2. Deploy `/v{n+1}` in parallel
  3. Mark `/v{n}` as deprecated with `Deprecation` and `Sunset` headers
  4. Monitor usage metrics
  5. Retire `/v{n}` after 6 months and zero critical usage

### 14.3 Protobuf Evolution

- **Rules:**
  - Use `reserved` for removed fields
  - Never reuse field numbers
  - Additive changes only (new optional fields)
  - Use `oneof` for variants
- **Backward Compatibility:** Clients must ignore unknown fields.
- **Forward Compatibility:** Servers must handle missing optional fields.

### 14.4 API Contract Testing

- **Tools:** Protolock (protobuf), OpenAPI diff
- **Gates:** CI fails on breaking changes without version bump
- **Documentation:** Auto-generate API docs from protobufs and OpenAPI specs

---

## 15. Transform Profiles & Content Policy

### 15.1 Transformation Profile Schema

Profiles are defined declaratively (YAML/DB):

```yaml
profileId: image.thumbnail
type: image
enabled: true
steps:
  - action: resize
    params:
      maxWidth: 256
      maxHeight: 256
      mode: contain
  - action: strip_exif
  - action: convert
    params:
      format: webp
      quality: 85
limits:
  maxInputSize: 50MB
  timeout: 30s
  maxDimensions: 8192x8192
outputs:
  - suffix: _thumb
    format: webp
```

### 15.2 Content Policy

- **Allowed MIME Types (default):** `image/*`, `application/pdf`, `video/mp4`, `video/webm`
- **Blocked Extensions:** `.exe`, `.bat`, `.sh`, `.cmd`, `.com`, `.scr`, `.vbs`
- **Size Limits:** 
  - Free tier: 50MB/object, 1GB/month
  - Pro tier: 500MB/object, 100GB/month
- **Dimension Limits (images):** Max 8192x8192 px
- **Duration Limits (video):** Max 60 minutes

### 15.3 Extension Points

- **DLP (Data Loss Prevention):** Scan for PII, credit cards, secrets (future).
- **NSFW Classification:** Optional content moderation via ML model (future).

---

---

## 16. Requirements Traceability Matrix (RTM)

Links each requirement to business goals, risks, verification methods, and implementation components.

| Req ID | Title | Rationale | Risk | Verification | Test Case | Component | Status |
|--------|-------|-----------|------|--------------|-----------|-----------|--------|
| **A-01** | Username/password login with rate limiting | Secure primary authentication | H | Integration + E2E | AUTH_TC_01 | BFF/Auth | ✅ Specified |
| **A-02** | Short-lived access tokens (≤15 min) | Minimize token theft impact | H | Unit + Integration | AUTH_TC_02 | Auth gRPC | ✅ Specified |
| **A-03** | Session binding & remote revocation | Multi-device security | M | Integration + E2E | AUTH_TC_03 | BFF/Session | ✅ Specified |
| **A-04** | TOTP enrollment & verification | Strong 2FA | H | Integration + E2E | AUTH_TC_04 | Auth gRPC | ✅ Specified |
| **A-07** | Password breach screening | Prevent compromised credentials | H | Unit + Integration | AUTH_TC_07 | Auth gRPC | ✅ Specified |
| **A-10** | API key creation with scopes | Service-to-service auth | M | Integration + E2E | APIKEY_TC_01 | Auth gRPC | ✅ Specified |
| **A-13** | RBAC with predefined roles | Authorization model | H | Unit + Integration | RBAC_TC_01 | Auth gRPC | ✅ Specified |
| **A-18** | Refresh token reuse detection | Prevent token theft | H | Integration | AUTH_TC_18 | Auth gRPC | ✅ Specified |
| **A-20** | Rate limiting per identity | DoS prevention | H | Integration | RATE_TC_01 | BFF | ✅ Specified |
| **A-21** | Account lockout on failed logins | Brute force protection | H | Integration + E2E | AUTH_TC_21 | Auth gRPC | ✅ Specified |
| **B-27** | Tenant/user quotas | Prevent abuse | H | Integration | QUOTA_TC_01 | Upload gRPC | ✅ Specified |
| **B-26** | Presigned URL with TTL ≤10 min | Limit exposure window | M | Integration | UP_TC_02 | Upload gRPC | ✅ Specified |
| **B-28** | AV scan before publish | Malware prevention | H | Integration + E2E | UP_TC_11 | Lambda/Transform | ✅ Specified |
| **B-31** | Zip bomb detection | DoS prevention | M | Integration | UP_TC_15 | Lambda/Validation | ✅ Specified |
| **B-32** | Checksum validation | Integrity assurance | M | Integration | UP_TC_16 | Upload gRPC | ✅ Specified |
| **B-33** | Deterministic key layout | Path traversal prevention | H | Unit + Integration | UP_TC_17 | Upload gRPC | ✅ Specified |
| **C-49** | Stripe integration with PCI-DSS | Secure payment processing | H | Integration | PAY_TC_01 | Payment gRPC | ✅ Specified |
| **C-52** | Payment tokenization | Never store card data | H | Unit + Integration | PAY_TC_02 | Payment gRPC | ✅ Specified |
| **C-54** | Predefined pricing plans | Revenue model | M | Integration + E2E | SUB_TC_01 | Payment gRPC | ✅ Specified |
| **C-63** | Usage metering | Accurate billing | H | Integration | METER_TC_01 | Payment gRPC | ✅ Specified |
| **D-71** | Multi-channel notifications | User engagement | M | Integration + E2E | NOTIF_TC_01 | Notification gRPC | ✅ Specified |
| **D-80** | Notification queue with retry | Reliable delivery | M | Integration | NOTIF_TC_05 | Notification gRPC | ✅ Specified |
| **D-85** | Webhook signature verification | Security | H | Integration | WH_TC_01 | Notification gRPC | ✅ Specified |
| **E-96** | Email queue processing | Reliable delivery | M | Integration | EMAIL_TC_01 | Email Service | ✅ Specified |
| **E-104** | Transaction isolation levels | Data consistency | H | Integration | TXN_TC_01 | DB Layer | ✅ Specified |
| **E-106** | Idempotency keys | Exactly-once semantics | H | Integration | TXN_TC_05 | BFF | ✅ Specified |

### RTM Usage

- **Risk Levels:** L (Low), M (Medium), H (High)
- **Status:** ✅ Specified, 🚧 In Progress, ✔️ Implemented, ❌ Blocked
- **Verification:** Unit (isolated), Integration (service-to-service), E2E (full stack), Chaos (fault injection)

---

## 17. Representative API Contracts

### 13.1 Login (BFF)

#### Request

```http
POST /v1/auth/login
Content-Type: application/json

{
  "email": "user@example.com",
  "password": "••••••••"
}
```

#### Response (2FA required)

```json
{
  "status": "mfa_required",
  "factors": ["totp", "otp_email"],
  "challengeId": "ch_123"
}
```

#### Response (success)

```json
{
  "accessToken": "jwt...",
  "refreshToken": "opaque...",
  "expiresIn": 900
}
```

---

### 13.2 Presign Upload

#### Request

```http
POST /v1/upload/presign
Content-Type: application/json

{
  "filename": "report.pdf",
  "contentType": "application/pdf",
  "size": 5242880,
  "checksum": "sha256:..."
}
```

#### Response

```json
{
  "objectId": "obj_abc",
  "storage": "s3",
  "bucket": "tenant-123",
  "key": "uploads/2025/11/obj_abc.pdf",
  "presigned": {
    "url": "https://s3...",
    "method": "PUT",
    "headers": {
      "Content-Type": "application/pdf",
      "x-amz-checksum-sha256": "..."
    }
  },
  "constraints": {
    "maxSize": 104857600,
    "allowedTypes": ["application/pdf", "image/*"]
  }
}
```

---

### 13.3 Complete Upload

#### Request

```http
POST /v1/upload/complete
Content-Type: application/json

{
  "objectId": "obj_abc",
  "etag": "\"d41d8cd98f...\"",
  "parts": [
    {
      "partNumber": 1,
      "etag": "..."
    }
  ]
}
```

#### Response

```json
{
  "status": "VALIDATING"
}
```

---

### 17.4 Create Subscription (Payment)

#### Request

```http
POST /v1/subscriptions
Content-Type: application/json

{
  "planId": "pro_monthly",
  "paymentMethodId": "pm_123456",
  "trialDays": 14
}
```

#### Response

```json
{
  "id": "sub_abc123",
  "tenantId": "tenant_xyz",
  "plan": {
    "id": "pro_monthly",
    "name": "Pro",
    "amount": 2900,
    "currency": "usd",
    "interval": "month"
  },
  "status": "trialing",
  "currentPeriodStart": "2025-11-10T10:00:00Z",
  "currentPeriodEnd": "2025-12-10T10:00:00Z",
  "trialEnd": "2025-11-24T10:00:00Z",
  "cancelAtPeriodEnd": false
}
```

---

### 17.5 Send Notification

#### Request

```http
POST /v1/notifications
Content-Type: application/json

{
  "userId": "user_123",
  "type": "upload_completed",
  "channels": ["email", "in_app"],
  "data": {
    "objectId": "obj_abc",
    "filename": "report.pdf",
    "size": 5242880
  }
}
```

#### Response

```json
{
  "id": "notif_xyz789",
  "status": "queued",
  "deliveries": [
    {
      "channel": "email",
      "status": "queued",
      "estimatedDelivery": "2025-11-10T10:00:30Z"
    },
    {
      "channel": "in_app",
      "status": "delivered",
      "deliveredAt": "2025-11-10T10:00:01Z"
    }
  ]
}
```

---

### 17.6 Create Webhook

#### Request

```http
POST /v1/webhooks
Content-Type: application/json

{
  "url": "https://example.com/webhooks/uploads",
  "events": ["upload.completed", "upload.failed"],
  "description": "Upload notifications"
}
```

#### Response

```json
{
  "id": "wh_abc123",
  "tenantId": "tenant_xyz",
  "url": "https://example.com/webhooks/uploads",
  "events": ["upload.completed", "upload.failed"],
  "secret": "whsec_abc123...",
  "status": "active",
  "createdAt": "2025-11-10T10:00:00Z"
}
```

**Note:** Secret is shown only once. Use it to verify webhook signatures.

---

## 18. Admin & Tenant Operations

### 18.1 Tenant Management UI

- **Quota Editor:** View and adjust tenant quotas (daily/monthly bytes, object count, max size)
- **API Key Management:** List, create, rotate, and revoke API keys with scope visualization
- **Session Dashboard:** View active sessions with device hints, location (IP-based), last activity
- **Audit Export:** Export audit logs for date range (CSV/JSON) with filtering
- **Subscription Management:** View current plan, usage stats, upgrade/downgrade options, billing history
- **Payment Methods:** List cards, add new payment method, set default, remove old cards
- **Invoice History:** View all invoices, download PDFs, see payment status
- **Notification Center:** Configure webhook endpoints, test deliveries, view webhook logs
- **Email Templates:** Preview and customize email templates (admin-only)
- **Bulk Operations:**
  - Revoke all sessions for specific user or entire tenant
  - Batch API key rotation
  - Bulk user role assignment
  - Mass notification send (announcements)

### 18.2 User Self-Service

- **Profile Management:** Update email (with verification), change password, view sessions
- **2FA Setup:** QR code enrollment, backup codes download
- **Device Management:** Nickname devices, revoke individual sessions
- **Data Export:** Trigger GDPR-compliant data export
- **Billing Portal:** View subscription, update payment method, download invoices (Stripe portal)
- **Usage Dashboard:** Real-time usage vs. plan limits with charts
- **Notification Preferences:** Configure channels, quiet hours, frequency (daily digest)

---

## 19. Developer Experience (DX)

### 19.1 API Documentation

- **OpenAPI Spec:** Auto-generated from FastAPI BFF endpoints
- **Protobuf Docs:** Published via Buf Schema Registry
- **SDK Stubs:** Generated for TypeScript, Python, Go
- **Postman Collection:** Pre-configured with auth and sample requests
- **VS Code REST Client:** `.http` files for quick testing

### 19.2 Local Development

- **Docker Compose:** Full stack (BFF, gRPC, Postgres, Redis, LocalStack S3)
- **Seed Data:** Sample tenants, users, API keys for testing
- **Mock Services:** Stubbed SMS/Email providers
- **Hot Reload:** Live reload for BFF (FastAPI) and UI (Vite)

---

## 20. Cost Guardrails & Budgets

### 20.1 Budget Alerts

| Service | Budget (monthly) | Alert Threshold | Action |
|---------|------------------|-----------------|--------|
| **S3 Storage** | $5,000 | 80% | Email to ops + Slack |
| **S3 Requests** | $2,000 | 80% | Email to ops + Slack |
| **SMS/Email** | $1,500 | 80% | Email to ops, throttle non-critical |
| **AV Scanning** | $800 | 90% | Email to ops + SEV-2 |
| **Lambda Transforms** | $3,000 | 80% | Email to ops + Slack |

### 20.2 Tenant Cost Attribution

- Tag all resources with `tenantId` for cost allocation
- Monthly cost reports per tenant (S3 storage, bandwidth, transform compute)
- Automated billing adjustments for Pro/Enterprise tiers

---

## 21. Breaking Changes Ledger

### Version 0.1 → 0.2 (Target: Q2 2026)

| Change | Type | Reason | Migration Path | Deprecation Date |
|--------|------|--------|----------------|------------------|
| `/v1/auth/login` → `/v2/auth/login` | Breaking | Add device fingerprint | Use `/v2`; `/v1` supported until Aug 2026 | Feb 2026 |
| `ApiKey.scope` string → `scopes[]` | Breaking | Multi-scope support | Migrate to array format | TBD |
| Add `Idempotency-Key` header requirement | Breaking | Ensure idempotency | Include header on all POSTs | TBD |

### Change Control Process

1. Propose change in RFC (Request for Comments)
2. Technical review + security review
3. Add to Breaking Changes Ledger
4. Announce 3 months prior via changelog, email, in-app banner
5. Deploy parallel version
6. Monitor adoption metrics
7. Retire old version after 6 months and < 1% usage

---

## 22. Open Issues & Future Enhancements

### 22.1 Open Issues (to be finalized)

- [ ] Choice of SMS provider(s) and fallback strategy (Twilio vs. AWS SNS vs. multi-provider)
- [ ] WebAuthn support as alternative second factor (passkeys, FIDO2)
- [ ] Customer-managed keys (CMK) per tenant vs. shared KMS keys—pricing/ops trade-offs
- [ ] DLP (Data Loss Prevention) integration for upload scanning
- [ ] Real-time collaboration features (WebSocket auth, presence)
- [ ] Payment gateway redundancy (Stripe primary, PayPal backup)
- [ ] Push notification platform choice (OneSignal vs. FCM/APNs direct)
- [ ] Email service provider failover strategy (SendGrid → AWS SES)

### 22.2 Future Enhancements (v0.3+)

- [ ] **WebAuthn/Passkeys:** Passwordless authentication with FIDO2
- [ ] **Social Login:** OAuth 2.0 integration (Google, GitHub, Microsoft)
- [ ] **SCIM 2.0:** Automated user provisioning for enterprise SSO
- [ ] **Content Moderation:** NSFW classification, automated flagging
- [ ] **Edge Upload:** Cloudflare Workers for global upload ingress
- [ ] **Multi-Region:** Active-active deployment across AWS regions
- [ ] **Tenant Isolation:** Dedicated S3 buckets + KMS keys per tenant (Enterprise tier)
- [ ] **Advanced Analytics:** Usage dashboards, cost forecasting, anomaly detection
- [ ] **Invoice Customization:** Custom fields, logo, tax IDs per tenant
- [ ] **Dunning Management:** Advanced failed payment recovery workflows
- [ ] **SMS 2FA:** Alternative to email OTP for better security
- [ ] **Affiliate Program:** Referral tracking and commission management

---

## Appendices

### Appendix A: References

- **RFC 6238:** TOTP: Time-Based One-Time Password Algorithm
- **RFC 6749:** OAuth 2.0 Authorization Framework
- **RFC 7519:** JSON Web Token (JWT)
- **RFC 8725:** JWT Best Current Practices
- **NIST SP 800-63B:** Digital Identity Guidelines (Authentication)
- **GDPR:** General Data Protection Regulation (EU 2016/679)
- **PCI-DSS:** Payment Card Industry Data Security Standard
- **PSD2:** Payment Services Directive 2 (Strong Customer Authentication)
- **CAN-SPAM Act:** Email marketing compliance (US)
- **WCAG 2.1:** Web Content Accessibility Guidelines
- **STRIDE:** Threat Modeling Framework (Microsoft)
- **jwt-cpp Documentation:** https://thalhammer.github.io/jwt-cpp/
- **gRPC Authentication Guide:** https://grpc.io/docs/guides/auth/
- **OpenTelemetry:** https://opentelemetry.io/
- **cert-manager:** https://cert-manager.io/

### Appendix B: Glossary

- **KEK:** Key Encryption Key (wraps data encryption keys)
- **CMK:** Customer-Managed Key (tenant-specific KMS key)
- **Polyglot File:** File with multiple valid interpretations (e.g., JPG+JS)
- **k-Anon:** k-Anonymity (privacy model for breach password checking)
- **TOCTOU:** Time-of-Check Time-of-Use (race condition attack)
- **DPA:** Data Processing Agreement (GDPR contract)
- **SCA:** Strong Customer Authentication (3D Secure 2.0)
- **MRR:** Monthly Recurring Revenue
- **Churn:** Subscription cancellation rate
- **Dunning:** Process of communicating with customers about failed payments
- **JWT:** JSON Web Token (RFC 7519) - token-based authentication standard
- **mTLS:** Mutual Transport Layer Security - bidirectional certificate authentication
- **RS256:** RSA Signature with SHA-256 - asymmetric JWT signing algorithm
- **JTI:** JWT ID - unique identifier for token revocation
- **KID:** Key ID - identifies which key was used to sign JWT
- **Blacklist:** Redis-based list of revoked tokens (by JTI)
- **Token Rotation:** Issuing new refresh token and invalidating old one

### Appendix C: Contact & Ownership

- **Product Owner:** [Name] ([email])
- **Tech Lead (Auth):** [Name] ([email])
- **Tech Lead (Upload):** [Name] ([email])
- **Tech Lead (Payment):** [Name] ([email])
- **Tech Lead (Notifications):** [Name] ([email])
- **Security Reviewer:** [Name] ([email])
- **Compliance Officer:** [Name] ([email])

---

**Document Status:** Living Document — Updated Quarterly  
**Last Review:** 2025-11-10  
**Next Review:** 2026-02-10  
**Change Log:** See Git history for detailed change tracking
