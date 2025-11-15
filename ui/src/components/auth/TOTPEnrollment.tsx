/**
 * @file        TOTPEnrollment.tsx
 * @brief       TOTP 2FA enrollment component with QR code display
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

interface TOTPEnrollmentProps {
  onComplete: (backupCodes: string[]) => void
  onCancel: () => void
}

interface TOTPSecret {
  secret: string
  qr_code_url: string
  manual_entry_key: string
}

export function TOTPEnrollment({ onComplete, onCancel }: TOTPEnrollmentProps) {
  const [totpSecret, setTotpSecret] = useState<TOTPSecret | null>(null)
  const [verificationCode, setVerificationCode] = useState('')
  const [error, setError] = useState('')
  const [isLoading, setIsLoading] = useState(false)
  const [step, setStep] = useState<'scan' | 'verify'>('scan')

  useEffect(() => {
    const fetchTOTPSecret = async () => {
      try {
        const response = await apiClient.post('/v1/auth/totp/enroll')
        setTotpSecret(response.data)
      } catch (err: any) {
        setError(err.response?.data?.message || 'Failed to generate TOTP secret')
      }
    }

    fetchTOTPSecret()
  }, [])

  const handleVerify = async () => {
    if (!verificationCode || verificationCode.length !== 6) {
      setError('Please enter a valid 6-digit code')
      return
    }

    setError('')
    setIsLoading(true)

    try {
      const response = await apiClient.post('/v1/auth/totp/verify', {
        code: verificationCode,
      })

      const { backup_codes } = response.data
      onComplete(backup_codes)
    } catch (err: any) {
      setError(err.response?.data?.message || 'Invalid verification code. Please try again.')
    } finally {
      setIsLoading(false)
    }
  }

  const handleCopyKey = () => {
    if (totpSecret?.manual_entry_key) {
      navigator.clipboard.writeText(totpSecret.manual_entry_key)
    }
  }

  if (!totpSecret) {
    return (
      <div className="text-center py-8">
        <div className="inline-block animate-spin rounded-full h-8 w-8 border-b-2 border-ember-orange" />
        <p className="mt-4 text-gray-600">Generating TOTP secret...</p>
      </div>
    )
  }

  return (
    <div className="space-y-6">
      {error && (
        <Alert type="error" onClose={() => setError('')}>
          {error}
        </Alert>
      )}

      {step === 'scan' && (
        <>
          <div className="text-center">
            <h3 className="text-lg font-semibold text-gray-900">
              Scan QR Code with Authenticator App
            </h3>
            <p className="mt-2 text-sm text-gray-600">
              Use Google Authenticator, Authy, or any TOTP-compatible app
            </p>
          </div>

          {/* QR Code */}
          <div className="flex justify-center bg-white p-4 rounded-lg border-2 border-gray-200">
            <img
              src={totpSecret.qr_code_url}
              alt="TOTP QR Code"
              className="w-64 h-64"
            />
          </div>

          {/* Manual Entry */}
          <div className="bg-gray-50 p-4 rounded-lg">
            <p className="text-sm font-medium text-gray-700 mb-2">
              Can't scan the QR code? Enter this key manually:
            </p>
            <div className="flex items-center gap-2">
              <code className="flex-1 px-3 py-2 bg-white border border-gray-300 rounded font-mono text-sm break-all">
                {totpSecret.manual_entry_key}
              </code>
              <Button variant="outline" onClick={handleCopyKey}>
                Copy
              </Button>
            </div>
          </div>

          <Button fullWidth onClick={() => setStep('verify')}>
            Continue to Verification
          </Button>

          <Button fullWidth variant="outline" onClick={onCancel}>
            Cancel
          </Button>
        </>
      )}

      {step === 'verify' && (
        <>
          <div className="text-center">
            <h3 className="text-lg font-semibold text-gray-900">Enter Verification Code</h3>
            <p className="mt-2 text-sm text-gray-600">
              Enter the 6-digit code from your authenticator app
            </p>
          </div>

          <Input
            label="Verification Code"
            type="text"
            inputMode="numeric"
            pattern="[0-9]{6}"
            maxLength={6}
            value={verificationCode}
            onChange={(e) => setVerificationCode(e.target.value.replace(/\D/g, ''))}
            placeholder="000000"
            autoFocus
            disabled={isLoading}
          />

          <div className="flex gap-3">
            <Button fullWidth onClick={handleVerify} isLoading={isLoading}>
              Verify and Enable 2FA
            </Button>
            <Button fullWidth variant="outline" onClick={() => setStep('scan')} disabled={isLoading}>
              Back
            </Button>
          </div>
        </>
      )}
    </div>
  )
}
