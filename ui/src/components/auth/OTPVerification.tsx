/**
 * @file        OTPVerification.tsx
 * @brief       SMS/Email OTP verification component (alternative to TOTP)
 *
 * @copyright   (c) 2025 FtsCoDe GmbH. All rights reserved.
 * @author      Heinstein F.
 * @date        2025-11-15
 */

import { useState, useEffect } from 'react'
import { apiClient } from '@/api/client'
import { Input } from '@/components/ui/Input'
import { Button } from '@/components/ui/Button'
import { Alert } from '@/components/ui/Alert'

interface OTPVerificationProps {
  method: 'email' | 'sms'
  onVerify: (code: string) => Promise<void>
  onCancel: () => void
  onResend?: () => Promise<void>
}

export function OTPVerification({ method, onVerify, onCancel, onResend }: OTPVerificationProps) {
  const [code, setCode] = useState('')
  const [error, setError] = useState('')
  const [isLoading, setIsLoading] = useState(false)
  const [resendCooldown, setResendCooldown] = useState(0)
  const [resendCount, setResendCount] = useState(0)

  useEffect(() => {
    if (resendCooldown > 0) {
      const timer = setTimeout(() => setResendCooldown(resendCooldown - 1), 1000)
      return () => clearTimeout(timer)
    }
  }, [resendCooldown])

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault()
    setError('')

    if (!code || code.length !== 6) {
      setError('Please enter a valid 6-digit code')
      return
    }

    setIsLoading(true)

    try {
      await onVerify(code)
    } catch (err: any) {
      setError(err.response?.data?.message || 'Invalid code. Please try again.')
    } finally {
      setIsLoading(false)
    }
  }

  const handleResend = async () => {
    if (resendCooldown > 0) return

    setError('')
    setIsLoading(true)

    try {
      if (onResend) {
        await onResend()
      } else {
        await apiClient.post('/v1/auth/otp/resend', { method })
      }

      setResendCount(resendCount + 1)
      // Exponential backoff: 30s, 60s, 120s
      setResendCooldown(30 * Math.pow(2, Math.min(resendCount, 2)))
      setCode('')
    } catch (err: any) {
      setError(err.response?.data?.message || 'Failed to resend code. Please try again.')
    } finally {
      setIsLoading(false)
    }
  }

  return (
    <div className="space-y-6">
      <div className="text-center">
        <h3 className="text-lg font-semibold text-gray-900">Enter Verification Code</h3>
        <p className="mt-2 text-sm text-gray-600">
          We've sent a 6-digit code to your {method === 'email' ? 'email address' : 'phone number'}.
          The code will expire in 10 minutes.
        </p>
      </div>

      {error && (
        <Alert type="error" onClose={() => setError('')}>
          {error}
        </Alert>
      )}

      <form onSubmit={handleSubmit} className="space-y-4">
        <Input
          label="Verification Code"
          type="text"
          inputMode="numeric"
          pattern="[0-9]{6}"
          maxLength={6}
          value={code}
          onChange={(e) => setCode(e.target.value.replace(/\D/g, ''))}
          placeholder="000000"
          autoFocus
          disabled={isLoading}
          helperText="Enter the 6-digit code you received"
        />

        <div className="flex gap-3">
          <Button type="submit" fullWidth isLoading={isLoading}>
            Verify
          </Button>
          <Button type="button" fullWidth variant="outline" onClick={onCancel} disabled={isLoading}>
            Cancel
          </Button>
        </div>
      </form>

      {/* Resend Code */}
      <div className="text-center">
        <button
          type="button"
          onClick={handleResend}
          disabled={resendCooldown > 0 || isLoading}
          className="text-sm text-ember-orange hover:text-opacity-80 font-medium disabled:opacity-50 disabled:cursor-not-allowed"
        >
          {resendCooldown > 0
            ? `Resend code in ${resendCooldown}s`
            : resendCount > 0
            ? 'Resend code again'
            : "Didn't receive code? Resend"}
        </button>
      </div>

      {/* Rate Limit Warning */}
      {resendCount >= 3 && (
        <Alert type="warning">
          You've requested multiple codes. If you continue to have issues, please contact support.
        </Alert>
      )}
    </div>
  )
}
