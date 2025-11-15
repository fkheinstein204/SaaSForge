/**
 * @file        LoginPage.tsx
 * @brief       Login page with email/password and OAuth authentication
 *
 * @copyright   (c) 2025 FtsCoDe GmbH. All rights reserved.
 * @author      Heinstein F.
 * @date        2025-11-15
 */

import { useState } from 'react'
import { Link, useNavigate } from 'react-router-dom'
import { useAuthStore } from '@/store/authStore'
import { apiClient } from '@/api/client'
import { Input } from '@/components/ui/Input'
import { Button } from '@/components/ui/Button'
import { Alert } from '@/components/ui/Alert'
import { OAuthButtonGroup } from '@/components/auth/OAuthButton'
import { TwoFactorModal } from '@/components/auth/TwoFactorModal'
import { useToast } from '../contexts/ToastContext'

export function LoginPage() {
  const [email, setEmail] = useState('')
  const [password, setPassword] = useState('')
  const [error, setError] = useState('')
  const [isLoading, setIsLoading] = useState(false)
  const [show2FAModal, setShow2FAModal] = useState(false)
  const [tempSessionToken, setTempSessionToken] = useState('')
  const navigate = useNavigate()
  const { login, initiateOAuth } = useAuthStore()
  const { success, error: showErrorToast } = useToast()

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault()
    setError('')
    setIsLoading(true)

    try {
      await login(email, password)
      success('Login successful', 'Welcome back!')
      navigate('/')
    } catch (err: any) {
      // Check if 2FA is required
      if (err.response?.status === 403 && err.response?.data?.requires_2fa) {
        setTempSessionToken(err.response.data.temp_token)
        setShow2FAModal(true)
        setIsLoading(false)
        return
      }

      const errorMessage = err.response?.data?.message || 'Invalid credentials. Please try again.'
      setError(errorMessage)
      showErrorToast('Login failed', errorMessage)
    } finally {
      setIsLoading(false)
    }
  }

  const handle2FAVerify = async (code: string, useBackupCode: boolean) => {
    const endpoint = useBackupCode ? '/v1/auth/2fa/backup-code' : '/v1/auth/2fa/verify'

    const response = await apiClient.post(endpoint, {
      temp_token: tempSessionToken,
      code,
    })

    const { access_token, refresh_token, user_id, tenant_id, email: userEmail } = response.data

    // Update auth store
    useAuthStore.setState({
      user: { id: user_id, email: userEmail, tenantId: tenant_id, totpEnabled: true },
      accessToken: access_token,
      refreshToken: refresh_token,
    })

    setShow2FAModal(false)
    success('2FA verified', 'Authentication successful!')
    navigate('/')
  }

  const handleOAuth = async (provider: 'google' | 'github' | 'microsoft') => {
    try {
      setError('')
      await initiateOAuth(provider)
      // User will be redirected to OAuth provider
    } catch (err: any) {
      const errorMessage = err.response?.data?.message || 'OAuth authentication failed. Please try again.'
      setError(errorMessage)
      showErrorToast('OAuth failed', errorMessage)
    }
  }

  return (
    <>
      {show2FAModal && (
        <TwoFactorModal
          onVerify={handle2FAVerify}
          onCancel={() => {
            setShow2FAModal(false)
            setTempSessionToken('')
          }}
        />
      )}

      <div className="min-h-screen flex items-center justify-center bg-cloud-white px-4">
        <div className="max-w-md w-full space-y-8 p-8 bg-white rounded-lg shadow-md">
        {/* Header */}
        <div className="text-center">
          <h2 className="text-3xl font-bold text-forge-blue">SaaSForge</h2>
          <p className="mt-2 text-steel-gray">Sign in to your account</p>
        </div>

        {/* Error Alert */}
        {error && (
          <Alert type="error" onClose={() => setError('')}>
            {error}
          </Alert>
        )}

        {/* Login Form */}
        <form className="mt-8 space-y-6" onSubmit={handleSubmit}>
          <Input
            label="Email address"
            type="email"
            autoComplete="email"
            required
            value={email}
            onChange={(e) => setEmail(e.target.value)}
            placeholder="you@example.com"
            disabled={isLoading}
          />

          <Input
            label="Password"
            type="password"
            autoComplete="current-password"
            required
            value={password}
            onChange={(e) => setPassword(e.target.value)}
            placeholder="••••••••"
            disabled={isLoading}
          />

          <div className="flex items-center justify-between">
            <div className="flex items-center">
              <input
                id="remember-me"
                name="remember-me"
                type="checkbox"
                className="h-4 w-4 text-ember-orange focus:ring-ember-orange border-gray-300 rounded"
              />
              <label htmlFor="remember-me" className="ml-2 block text-sm text-gray-700">
                Remember me
              </label>
            </div>

            <div className="text-sm">
              <Link
                to="/forgot-password"
                className="font-medium text-ember-orange hover:text-opacity-80"
              >
                Forgot password?
              </Link>
            </div>
          </div>

          <Button type="submit" fullWidth isLoading={isLoading}>
            Sign in
          </Button>
        </form>

        {/* OAuth Buttons */}
        <OAuthButtonGroup onAuth={handleOAuth} disabled={isLoading} />

        {/* Register Link */}
        <div className="text-center text-sm">
          <span className="text-gray-600">Don't have an account? </span>
          <Link to="/register" className="font-medium text-ember-orange hover:text-opacity-80">
            Sign up
          </Link>
        </div>
        </div>
      </div>
    </>
  )
}
