/**
 * @file        OAuthCallbackPage.tsx
 * @brief       OAuth callback handler page
 *
 * @copyright   (c) 2025 FtsCoDe GmbH. All rights reserved.
 * @author      Heinstein F.
 * @date        2025-11-15
 */

import { useEffect, useState } from 'react'
import { useNavigate, useSearchParams } from 'react-router-dom'
import { useAuthStore } from '@/store/authStore'
import { apiClient } from '@/api/client'
import { Alert } from '@/components/ui/Alert'

export function OAuthCallbackPage() {
  const [searchParams] = useSearchParams()
  const [error, setError] = useState('')
  const navigate = useNavigate()

  useEffect(() => {
    const handleCallback = async () => {
      const code = searchParams.get('code')
      const state = searchParams.get('state')
      const errorParam = searchParams.get('error')
      const provider = searchParams.get('provider')

      if (errorParam) {
        setError(`OAuth authentication failed: ${errorParam}`)
        setTimeout(() => navigate('/login'), 3000)
        return
      }

      if (!code || !state || !provider) {
        setError('Invalid OAuth callback parameters')
        setTimeout(() => navigate('/login'), 3000)
        return
      }

      try {
        // Exchange authorization code for tokens
        const response = await apiClient.post(`/v1/oauth/${provider}/callback`, {
          code,
          state,
        })

        const { access_token, refresh_token, user_id, tenant_id, email } = response.data

        // Update auth store
        useAuthStore.setState({
          user: { id: user_id, email, tenantId: tenant_id },
          accessToken: access_token,
          refreshToken: refresh_token,
        })

        // Redirect to dashboard
        navigate('/')
      } catch (err: any) {
        setError(
          err.response?.data?.message || 'OAuth authentication failed. Please try again.'
        )
        setTimeout(() => navigate('/login'), 3000)
      }
    }

    handleCallback()
  }, [searchParams, navigate])

  return (
    <div className="min-h-screen flex items-center justify-center bg-cloud-white px-4">
      <div className="max-w-md w-full space-y-8 p-8 bg-white rounded-lg shadow-md text-center">
        {error ? (
          <>
            <Alert type="error">{error}</Alert>
            <p className="text-sm text-gray-500">Redirecting to login...</p>
          </>
        ) : (
          <>
            <div className="flex justify-center">
              <svg
                className="animate-spin h-12 w-12 text-ember-orange"
                xmlns="http://www.w3.org/2000/svg"
                fill="none"
                viewBox="0 0 24 24"
              >
                <circle
                  className="opacity-25"
                  cx="12"
                  cy="12"
                  r="10"
                  stroke="currentColor"
                  strokeWidth="4"
                />
                <path
                  className="opacity-75"
                  fill="currentColor"
                  d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"
                />
              </svg>
            </div>
            <p className="text-lg font-medium text-gray-900">Completing sign in...</p>
            <p className="text-sm text-gray-500">Please wait while we authenticate you</p>
          </>
        )}
      </div>
    </div>
  )
}
