// Common types used across the application

export interface ApiError {
  code: string
  message: string
  correlationId: string
  details?: Record<string, any>
}

export interface PaginatedResponse<T> {
  data: T[]
  total: number
  page: number
  page_size: number
}

export interface UploadObject {
  id: string
  filename: string
  size: number
  contentType: string
  url: string
  createdAt: string
}

export interface Subscription {
  id: string
  planId: string
  status: 'active' | 'past_due' | 'canceled' | 'unpaid' | 'trialing'
  currentPeriodStart: string
  currentPeriodEnd: string
  cancelAt?: string
}

export interface Notification {
  id: string
  channel: 'email' | 'sms' | 'push' | 'webhook'
  status: 'pending' | 'sent' | 'delivered' | 'failed' | 'bounced'
  createdAt: string
  sentAt?: string
  deliveredAt?: string
}
