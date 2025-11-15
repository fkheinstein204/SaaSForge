/**
 * @file        DashboardPage.tsx
 * @brief       Main dashboard page with metrics and activity
 *
 * @copyright   (c) 2025 FtsCoDe GmbH. All rights reserved.
 * @author      Heinstein F.
 * @date        2025-11-15
 */

import { useEffect, useState } from 'react'
import { Link } from 'react-router-dom'
import { apiClient } from '@/api/client'
import { AppLayout } from '@/components/layout/AppLayout'
import { MetricCard } from '@/components/dashboard/MetricCard'
import { Alert } from '@/components/ui/Alert'
import { useAuthStore } from '@/store/authStore'

interface DashboardStats {
  uploads: {
    total: number
    this_month: number
    trend: number
  }
  storage: {
    used_bytes: number
    quota_bytes: number
    percentage: number
  }
  subscription: {
    plan: string
    status: string
    mrr: number
  }
  api_calls: {
    total: number
    this_month: number
    trend: number
  }
}

export function DashboardPage() {
  const [stats, setStats] = useState<DashboardStats | null>(null)
  const [isLoading, setIsLoading] = useState(true)
  const [error, setError] = useState('')
  const user = useAuthStore((state) => state.user)

  useEffect(() => {
    const fetchStats = async () => {
      try {
        const response = await apiClient.get('/v1/dashboard/stats')
        setStats(response.data)
      } catch (err: any) {
        // Use mock data for now
        setStats({
          uploads: { total: 142, this_month: 23, trend: 12.5 },
          storage: { used_bytes: 2147483648, quota_bytes: 10737418240, percentage: 20 },
          subscription: { plan: 'Pro', status: 'active', mrr: 29 },
          api_calls: { total: 8542, this_month: 1203, trend: -5.2 },
        })
      } finally {
        setIsLoading(false)
      }
    }

    fetchStats()
  }, [])

  const formatBytes = (bytes: number): string => {
    if (bytes === 0) return '0 B'
    const k = 1024
    const sizes = ['B', 'KB', 'MB', 'GB', 'TB']
    const i = Math.floor(Math.log(bytes) / Math.log(k))
    return `${(bytes / Math.pow(k, i)).toFixed(2)} ${sizes[i]}`
  }

  return (
    <AppLayout title="Dashboard">
      <div className="space-y-8">
        {/* Welcome Banner */}
        <div className="bg-gradient-to-r from-forge-blue to-blue-700 rounded-lg shadow-md p-8 text-white">
          <h2 className="text-2xl font-bold">Welcome back!</h2>
          <p className="mt-2 text-blue-100">
            Here's what's happening with your account today.
          </p>
        </div>

        {/* 2FA Warning */}
        {!user?.totpEnabled && (
          <Alert type="warning" title="Secure Your Account">
            Two-factor authentication is not enabled. We strongly recommend enabling 2FA to
            protect your account.{' '}
            <Link to="/settings/2fa" className="underline font-semibold">
              Enable 2FA now
            </Link>
          </Alert>
        )}

        {/* Error Alert */}
        {error && (
          <Alert type="error" onClose={() => setError('')}>
            {error}
          </Alert>
        )}

        {/* Loading State */}
        {isLoading && (
          <div className="text-center py-12">
            <div className="inline-block animate-spin rounded-full h-12 w-12 border-b-2 border-ember-orange" />
            <p className="mt-4 text-gray-600">Loading dashboard...</p>
          </div>
        )}

        {/* Metrics Grid */}
        {!isLoading && stats && (
          <>
            <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-6">
              <MetricCard
                title="Total Uploads"
                value={stats.uploads.total.toLocaleString()}
                icon={
                  <svg className="h-6 w-6" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                    <path
                      strokeLinecap="round"
                      strokeLinejoin="round"
                      strokeWidth={2}
                      d="M7 16a4 4 0 01-.88-7.903A5 5 0 1115.9 6L16 6a5 5 0 011 9.9M15 13l-3-3m0 0l-3 3m3-3v12"
                    />
                  </svg>
                }
                trend={{
                  value: stats.uploads.trend,
                  isPositive: stats.uploads.trend >= 0,
                }}
                color="blue"
              />

              <MetricCard
                title="Storage Used"
                value={`${stats.storage.percentage}%`}
                icon={
                  <svg className="h-6 w-6" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                    <path
                      strokeLinecap="round"
                      strokeLinejoin="round"
                      strokeWidth={2}
                      d="M4 7v10c0 2.21 3.582 4 8 4s8-1.79 8-4V7M4 7c0 2.21 3.582 4 8 4s8-1.79 8-4M4 7c0-2.21 3.582-4 8-4s8 1.79 8 4"
                    />
                  </svg>
                }
                color="orange"
              />

              <MetricCard
                title="Current Plan"
                value={stats.subscription.plan}
                icon={
                  <svg className="h-6 w-6" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                    <path
                      strokeLinecap="round"
                      strokeLinejoin="round"
                      strokeWidth={2}
                      d="M3 10h18M7 15h1m4 0h1m-7 4h12a3 3 0 003-3V8a3 3 0 00-3-3H6a3 3 0 00-3 3v8a3 3 0 003 3z"
                    />
                  </svg>
                }
                color="green"
              />

              <MetricCard
                title="API Calls"
                value={stats.api_calls.total.toLocaleString()}
                icon={
                  <svg className="h-6 w-6" fill="none" viewBox="0 0 24 24" stroke="currentColor">
                    <path
                      strokeLinecap="round"
                      strokeLinejoin="round"
                      strokeWidth={2}
                      d="M13 10V3L4 14h7v7l9-11h-7z"
                    />
                  </svg>
                }
                trend={{
                  value: stats.api_calls.trend,
                  isPositive: stats.api_calls.trend >= 0,
                }}
                color="purple"
              />
            </div>

            {/* Storage Details */}
            <div className="bg-white rounded-lg shadow-md p-6 border border-gray-200">
              <h3 className="text-lg font-semibold text-gray-900 mb-4">Storage Usage</h3>
              <div className="space-y-4">
                <div>
                  <div className="flex justify-between text-sm text-gray-600 mb-2">
                    <span>
                      {formatBytes(stats.storage.used_bytes)} of{' '}
                      {formatBytes(stats.storage.quota_bytes)} used
                    </span>
                    <span>{stats.storage.percentage}%</span>
                  </div>
                  <div className="w-full bg-gray-200 rounded-full h-2">
                    <div
                      className="bg-ember-orange rounded-full h-2 transition-all duration-500"
                      style={{ width: `${Math.min(stats.storage.percentage, 100)}%` }}
                    />
                  </div>
                </div>
                {stats.storage.percentage > 80 && (
                  <Alert type="warning">
                    You're running low on storage space. Consider upgrading your plan or deleting
                    unused files.
                  </Alert>
                )}
              </div>
            </div>

            {/* Quick Actions */}
            <div className="bg-white rounded-lg shadow-md p-6 border border-gray-200">
              <h3 className="text-lg font-semibold text-gray-900 mb-4">Quick Actions</h3>
              <div className="grid grid-cols-1 md:grid-cols-3 gap-4">
                <Link
                  to="/uploads"
                  className="flex items-center p-4 border-2 border-gray-200 rounded-lg hover:border-ember-orange hover:bg-orange-50 transition-colors"
                >
                  <svg
                    className="h-8 w-8 text-ember-orange mr-3"
                    fill="none"
                    viewBox="0 0 24 24"
                    stroke="currentColor"
                  >
                    <path
                      strokeLinecap="round"
                      strokeLinejoin="round"
                      strokeWidth={2}
                      d="M7 16a4 4 0 01-.88-7.903A5 5 0 1115.9 6L16 6a5 5 0 011 9.9M15 13l-3-3m0 0l-3 3m3-3v12"
                    />
                  </svg>
                  <div>
                    <p className="font-semibold text-gray-900">Upload Files</p>
                    <p className="text-sm text-gray-600">Upload new files</p>
                  </div>
                </Link>

                <Link
                  to="/billing"
                  className="flex items-center p-4 border-2 border-gray-200 rounded-lg hover:border-ember-orange hover:bg-orange-50 transition-colors"
                >
                  <svg
                    className="h-8 w-8 text-ember-orange mr-3"
                    fill="none"
                    viewBox="0 0 24 24"
                    stroke="currentColor"
                  >
                    <path
                      strokeLinecap="round"
                      strokeLinejoin="round"
                      strokeWidth={2}
                      d="M3 10h18M7 15h1m4 0h1m-7 4h12a3 3 0 003-3V8a3 3 0 00-3-3H6a3 3 0 00-3 3v8a3 3 0 003 3z"
                    />
                  </svg>
                  <div>
                    <p className="font-semibold text-gray-900">Manage Billing</p>
                    <p className="text-sm text-gray-600">View invoices & plan</p>
                  </div>
                </Link>

                <Link
                  to="/settings/2fa"
                  className="flex items-center p-4 border-2 border-gray-200 rounded-lg hover:border-ember-orange hover:bg-orange-50 transition-colors"
                >
                  <svg
                    className="h-8 w-8 text-ember-orange mr-3"
                    fill="none"
                    viewBox="0 0 24 24"
                    stroke="currentColor"
                  >
                    <path
                      strokeLinecap="round"
                      strokeLinejoin="round"
                      strokeWidth={2}
                      d="M12 15v2m-6 4h12a2 2 0 002-2v-6a2 2 0 00-2-2H6a2 2 0 00-2 2v6a2 2 0 002 2zm10-10V7a4 4 0 00-8 0v4h8z"
                    />
                  </svg>
                  <div>
                    <p className="font-semibold text-gray-900">Security Settings</p>
                    <p className="text-sm text-gray-600">Manage 2FA</p>
                  </div>
                </Link>
              </div>
            </div>
          </>
        )}
      </div>
    </AppLayout>
  )
}
