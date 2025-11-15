/**
 * @file        ForgotPasswordPage.tsx
 * @brief       Password reset request page
 *
 * @copyright   (c) 2025 FtsCoDe GmbH. All rights reserved.
 * @author      Heinstein F.
 * @date        2025-11-15
 */

import { useState } from 'react'
import { Link } from 'react-router-dom'
import { useAuthStore } from '@/store/authStore'
import { Input } from '@/components/ui/Input'
import { Button } from '@/components/ui/Button'
import { Alert } from '@/components/ui/Alert'

export function ForgotPasswordPage() {
  const [email, setEmail] = useState('')
  const [error, setError] = useState('')
  const [success, setSuccess] = useState(false)
  const [isLoading, setIsLoading] = useState(false)
  const { requestPasswordReset } = useAuthStore()

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault()
    setError('')
    setSuccess(false)
    setIsLoading(true)

    try {
      await requestPasswordReset(email)
      setSuccess(true)
      setEmail('')
    } catch (err: any) {
      // For security, always show success message even if email doesn't exist
      setSuccess(true)
    } finally {
      setIsLoading(false)
    }
  }

  return (
    <div className="min-h-screen flex items-center justify-center bg-cloud-white px-4">
      <div className="max-w-md w-full space-y-8 p-8 bg-white rounded-lg shadow-md">
        {/* Header */}
        <div className="text-center">
          <h2 className="text-3xl font-bold text-forge-blue">Reset your password</h2>
          <p className="mt-2 text-steel-gray">
            Enter your email address and we'll send you a link to reset your password
          </p>
        </div>

        {/* Success Alert */}
        {success && (
          <Alert type="success">
            If an account exists with that email, we've sent password reset instructions. Please
            check your inbox and spam folder.
          </Alert>
        )}

        {/* Error Alert */}
        {error && (
          <Alert type="error" onClose={() => setError('')}>
            {error}
          </Alert>
        )}

        {!success && (
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

            <Button type="submit" fullWidth isLoading={isLoading}>
              Send reset link
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
