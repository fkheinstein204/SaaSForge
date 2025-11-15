/**
 * @file        ResetPasswordPage.tsx
 * @brief       Password reset confirmation page (with token from email)
 *
 * @copyright   (c) 2025 FtsCoDe GmbH. All rights reserved.
 * @author      Heinstein F.
 * @date        2025-11-15
 */

import { useState, useEffect } from 'react'
import { Link, useNavigate, useSearchParams } from 'react-router-dom'
import { useAuthStore } from '@/store/authStore'
import { Input } from '@/components/ui/Input'
import { Button } from '@/components/ui/Button'
import { Alert } from '@/components/ui/Alert'

export function ResetPasswordPage() {
  const [searchParams] = useSearchParams()
  const [password, setPassword] = useState('')
  const [confirmPassword, setConfirmPassword] = useState('')
  const [error, setError] = useState('')
  const [success, setSuccess] = useState(false)
  const [isLoading, setIsLoading] = useState(false)
  const navigate = useNavigate()
  const { resetPassword } = useAuthStore()

  const token = searchParams.get('token')

  useEffect(() => {
    if (!token) {
      setError('Invalid or missing reset token. Please request a new password reset link.')
    }
  }, [token])

  // Password validation
  const validatePassword = (pass: string): string | null => {
    if (pass.length < 12) {
      return 'Password must be at least 12 characters long'
    }
    if (!/[A-Z]/.test(pass)) {
      return 'Password must contain at least one uppercase letter'
    }
    if (!/[a-z]/.test(pass)) {
      return 'Password must contain at least one lowercase letter'
    }
    if (!/[0-9]/.test(pass)) {
      return 'Password must contain at least one number'
    }
    if (!/[^A-Za-z0-9]/.test(pass)) {
      return 'Password must contain at least one special character'
    }
    return null
  }

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault()
    setError('')

    if (!token) {
      setError('Invalid reset token')
      return
    }

    if (password !== confirmPassword) {
      setError('Passwords do not match')
      return
    }

    const passwordError = validatePassword(password)
    if (passwordError) {
      setError(passwordError)
      return
    }

    setIsLoading(true)

    try {
      await resetPassword(token, password)
      setSuccess(true)
      // Redirect to login after 3 seconds
      setTimeout(() => navigate('/login'), 3000)
    } catch (err: any) {
      const errorMessage =
        err.response?.data?.message ||
        'Password reset failed. The link may have expired. Please request a new one.'
      setError(errorMessage)
    } finally {
      setIsLoading(false)
    }
  }

  const passwordError = password ? validatePassword(password) : null

  return (
    <div className="min-h-screen flex items-center justify-center bg-cloud-white px-4">
      <div className="max-w-md w-full space-y-8 p-8 bg-white rounded-lg shadow-md">
        {/* Header */}
        <div className="text-center">
          <h2 className="text-3xl font-bold text-forge-blue">Set new password</h2>
          <p className="mt-2 text-steel-gray">Enter your new password below</p>
        </div>

        {/* Success Alert */}
        {success && (
          <Alert type="success">
            Password reset successful! Redirecting to login page...
          </Alert>
        )}

        {/* Error Alert */}
        {error && !success && (
          <Alert type="error" onClose={() => setError('')}>
            {error}
          </Alert>
        )}

        {!success && token && (
          <form className="mt-8 space-y-6" onSubmit={handleSubmit}>
            <Input
              label="New password"
              type="password"
              autoComplete="new-password"
              required
              value={password}
              onChange={(e) => setPassword(e.target.value)}
              placeholder="••••••••"
              disabled={isLoading}
              error={password && passwordError ? passwordError : undefined}
              helperText="At least 12 characters with uppercase, lowercase, number, and special character"
            />

            <Input
              label="Confirm new password"
              type="password"
              autoComplete="new-password"
              required
              value={confirmPassword}
              onChange={(e) => setConfirmPassword(e.target.value)}
              placeholder="••••••••"
              disabled={isLoading}
              error={
                confirmPassword && password !== confirmPassword
                  ? 'Passwords do not match'
                  : undefined
              }
            />

            <Button type="submit" fullWidth isLoading={isLoading}>
              Reset password
            </Button>
          </form>
        )}

        {/* Back to Login Link */}
        <div className="text-center text-sm">
          <Link to="/login" className="font-medium text-ember-orange hover:text-opacity-80">
            ← Back to sign in
          </Link>
        </div>
      </div>
    </div>
  )
}
