/**
 * @file        RegisterPage.tsx
 * @brief       User registration page with email/password and OAuth options
 *
 * @copyright   (c) 2025 FtsCoDe GmbH. All rights reserved.
 * @author      Heinstein F.
 * @date        2025-11-15
 */

import { useState } from 'react'
import { Link, useNavigate } from 'react-router-dom'
import { useAuthStore } from '@/store/authStore'
import { Input } from '@/components/ui/Input'
import { Button } from '@/components/ui/Button'
import { Alert } from '@/components/ui/Alert'
import { OAuthButtonGroup } from '@/components/auth/OAuthButton'

export function RegisterPage() {
  const [fullName, setFullName] = useState('')
  const [email, setEmail] = useState('')
  const [password, setPassword] = useState('')
  const [confirmPassword, setConfirmPassword] = useState('')
  const [error, setError] = useState('')
  const [isLoading, setIsLoading] = useState(false)
  const [agreedToTerms, setAgreedToTerms] = useState(false)
  const navigate = useNavigate()
  const { register, initiateOAuth } = useAuthStore()

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

    // Validation
    if (!agreedToTerms) {
      setError('You must agree to the Terms of Service and Privacy Policy')
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
      await register(email, password, fullName)
      navigate('/')
    } catch (err: any) {
      const errorMessage =
        err.response?.data?.message || 'Registration failed. Please try again.'
      setError(errorMessage)
    } finally {
      setIsLoading(false)
    }
  }

  const handleOAuth = async (provider: 'google' | 'github' | 'microsoft') => {
    try {
      setError('')
      await initiateOAuth(provider)
      // User will be redirected to OAuth provider
    } catch (err: any) {
      setError(err.response?.data?.message || 'OAuth authentication failed. Please try again.')
    }
  }

  // Real-time password validation feedback
  const passwordError = password ? validatePassword(password) : null

  return (
    <div className="min-h-screen flex items-center justify-center bg-cloud-white px-4 py-12">
      <div className="max-w-md w-full space-y-8 p-8 bg-white rounded-lg shadow-md">
        {/* Header */}
        <div className="text-center">
          <h2 className="text-3xl font-bold text-forge-blue">Create your account</h2>
          <p className="mt-2 text-steel-gray">Get started with SaaSForge</p>
        </div>

        {/* Error Alert */}
        {error && (
          <Alert type="error" onClose={() => setError('')}>
            {error}
          </Alert>
        )}

        {/* Registration Form */}
        <form className="mt-8 space-y-6" onSubmit={handleSubmit}>
          <Input
            label="Full name"
            type="text"
            autoComplete="name"
            required
            value={fullName}
            onChange={(e) => setFullName(e.target.value)}
            placeholder="John Doe"
            disabled={isLoading}
          />

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
            label="Confirm password"
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

          {/* Terms Agreement */}
          <div className="flex items-start">
            <div className="flex items-center h-5">
              <input
                id="terms"
                name="terms"
                type="checkbox"
                checked={agreedToTerms}
                onChange={(e) => setAgreedToTerms(e.target.checked)}
                className="h-4 w-4 text-ember-orange focus:ring-ember-orange border-gray-300 rounded"
              />
            </div>
            <div className="ml-3 text-sm">
              <label htmlFor="terms" className="text-gray-700">
                I agree to the{' '}
                <a href="/terms" className="text-ember-orange hover:text-opacity-80">
                  Terms of Service
                </a>{' '}
                and{' '}
                <a href="/privacy" className="text-ember-orange hover:text-opacity-80">
                  Privacy Policy
                </a>
              </label>
            </div>
          </div>

          <Button type="submit" fullWidth isLoading={isLoading}>
            Create account
          </Button>
        </form>

        {/* OAuth Buttons */}
        <OAuthButtonGroup onAuth={handleOAuth} disabled={isLoading} />

        {/* Login Link */}
        <div className="text-center text-sm">
          <span className="text-gray-600">Already have an account? </span>
          <Link to="/login" className="font-medium text-ember-orange hover:text-opacity-80">
            Sign in
          </Link>
        </div>
      </div>
    </div>
  )
}
