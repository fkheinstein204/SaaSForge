/**
 * @file        TwoFactorSettingsPage.tsx
 * @brief       Two-factor authentication settings and management page
 *
 * @copyright   (c) 2025 FtsCoDe GmbH. All rights reserved.
 * @author      Heinstein F.
 * @date        2025-11-15
 */

import { useState } from 'react'
import { useNavigate } from 'react-router-dom'
import { useAuthStore } from '@/store/authStore'
import { apiClient } from '@/api/client'
import { Button } from '@/components/ui/Button'
import { Alert } from '@/components/ui/Alert'
import { TOTPEnrollment } from '@/components/auth/TOTPEnrollment'
import { BackupCodes } from '@/components/auth/BackupCodes'

type SetupStep = 'settings' | 'enroll' | 'backup-codes' | 'complete'

export function TwoFactorSettingsPage() {
  const [step, setStep] = useState<SetupStep>('settings')
  const [backupCodes, setBackupCodes] = useState<string[]>([])
  const [error, setError] = useState('')
  const [success, setSuccess] = useState('')
  const [isDisabling, setIsDisabling] = useState(false)
  const navigate = useNavigate()
  const user = useAuthStore((state) => state.user)

  const totpEnabled = user?.totpEnabled || false

  const handleEnrollComplete = (codes: string[]) => {
    setBackupCodes(codes)
    setStep('backup-codes')
  }

  const handleBackupCodesComplete = () => {
    setStep('complete')
    // Update user state
    useAuthStore.setState((state) => ({
      user: state.user ? { ...state.user, totpEnabled: true } : null,
    }))
  }

  const handleDisable2FA = async () => {
    if (
      !confirm(
        'Are you sure you want to disable two-factor authentication? This will make your account less secure.'
      )
    ) {
      return
    }

    setIsDisabling(true)
    setError('')

    try {
      await apiClient.post('/v1/auth/totp/disable')
      useAuthStore.setState((state) => ({
        user: state.user ? { ...state.user, totpEnabled: false } : null,
      }))
      setSuccess('Two-factor authentication has been disabled')
    } catch (err: any) {
      setError(err.response?.data?.message || 'Failed to disable 2FA. Please try again.')
    } finally {
      setIsDisabling(false)
    }
  }

  const handleRegenerateBackupCodes = async () => {
    if (
      !confirm(
        'This will invalidate your existing backup codes. Are you sure you want to generate new ones?'
      )
    ) {
      return
    }

    try {
      const response = await apiClient.post('/v1/auth/totp/regenerate-backup-codes')
      setBackupCodes(response.data.backup_codes)
      setStep('backup-codes')
    } catch (err: any) {
      setError(
        err.response?.data?.message || 'Failed to regenerate backup codes. Please try again.'
      )
    }
  }

  if (step === 'enroll') {
    return (
      <div className="min-h-screen bg-cloud-white py-12 px-4">
        <div className="max-w-2xl mx-auto">
          <div className="bg-white rounded-lg shadow-md p-8">
            <h1 className="text-2xl font-bold text-gray-900 mb-6">Enable Two-Factor Authentication</h1>
            <TOTPEnrollment
              onComplete={handleEnrollComplete}
              onCancel={() => setStep('settings')}
            />
          </div>
        </div>
      </div>
    )
  }

  if (step === 'backup-codes') {
    return (
      <div className="min-h-screen bg-cloud-white py-12 px-4">
        <div className="max-w-2xl mx-auto">
          <div className="bg-white rounded-lg shadow-md p-8">
            <h1 className="text-2xl font-bold text-gray-900 mb-6">Save Your Backup Codes</h1>
            <BackupCodes codes={backupCodes} onComplete={handleBackupCodesComplete} />
          </div>
        </div>
      </div>
    )
  }

  if (step === 'complete') {
    return (
      <div className="min-h-screen bg-cloud-white py-12 px-4">
        <div className="max-w-2xl mx-auto">
          <div className="bg-white rounded-lg shadow-md p-8 text-center space-y-6">
            <div className="flex justify-center">
              <div className="rounded-full bg-green-100 p-3">
                <svg
                  className="h-12 w-12 text-green-600"
                  fill="none"
                  viewBox="0 0 24 24"
                  stroke="currentColor"
                >
                  <path
                    strokeLinecap="round"
                    strokeLinejoin="round"
                    strokeWidth={2}
                    d="M9 12l2 2 4-4m6 2a9 9 0 11-18 0 9 9 0 0118 0z"
                  />
                </svg>
              </div>
            </div>
            <div>
              <h1 className="text-2xl font-bold text-gray-900">Two-Factor Authentication Enabled!</h1>
              <p className="mt-2 text-gray-600">
                Your account is now protected with two-factor authentication.
              </p>
            </div>
            <Alert type="success">
              From now on, you'll need to enter a code from your authenticator app when signing in.
            </Alert>
            <Button fullWidth onClick={() => navigate('/')}>
              Return to Dashboard
            </Button>
          </div>
        </div>
      </div>
    )
  }

  return (
    <div className="min-h-screen bg-cloud-white py-12 px-4">
      <div className="max-w-2xl mx-auto">
        <div className="bg-white rounded-lg shadow-md p-8">
          <h1 className="text-2xl font-bold text-gray-900 mb-2">Two-Factor Authentication</h1>
          <p className="text-gray-600 mb-8">
            Add an extra layer of security to your account by enabling two-factor authentication.
          </p>

          {error && (
            <Alert type="error" onClose={() => setError('')}>
              {error}
            </Alert>
          )}

          {success && (
            <Alert type="success" onClose={() => setSuccess('')}>
              {success}
            </Alert>
          )}

          {/* Status */}
          <div className="bg-gray-50 p-6 rounded-lg mb-8">
            <div className="flex items-center justify-between">
              <div>
                <h3 className="text-lg font-semibold text-gray-900">Status</h3>
                <p className="text-sm text-gray-600 mt-1">
                  {totpEnabled
                    ? 'Two-factor authentication is currently enabled'
                    : 'Two-factor authentication is currently disabled'}
                </p>
              </div>
              <div>
                {totpEnabled ? (
                  <span className="inline-flex items-center px-3 py-1 rounded-full text-sm font-medium bg-green-100 text-green-800">
                    ✓ Enabled
                  </span>
                ) : (
                  <span className="inline-flex items-center px-3 py-1 rounded-full text-sm font-medium bg-yellow-100 text-yellow-800">
                    ⚠ Disabled
                  </span>
                )}
              </div>
            </div>
          </div>

          {/* How it Works */}
          <div className="mb-8">
            <h3 className="text-lg font-semibold text-gray-900 mb-4">How it works</h3>
            <div className="space-y-3">
              <div className="flex items-start">
                <span className="flex-shrink-0 w-6 h-6 rounded-full bg-ember-orange text-white flex items-center justify-center text-sm font-semibold mr-3">
                  1
                </span>
                <p className="text-gray-600">
                  Install an authenticator app like Google Authenticator or Authy on your phone
                </p>
              </div>
              <div className="flex items-start">
                <span className="flex-shrink-0 w-6 h-6 rounded-full bg-ember-orange text-white flex items-center justify-center text-sm font-semibold mr-3">
                  2
                </span>
                <p className="text-gray-600">
                  Scan the QR code with your authenticator app or enter the key manually
                </p>
              </div>
              <div className="flex items-start">
                <span className="flex-shrink-0 w-6 h-6 rounded-full bg-ember-orange text-white flex items-center justify-center text-sm font-semibold mr-3">
                  3
                </span>
                <p className="text-gray-600">
                  Enter the 6-digit code from your app to complete setup
                </p>
              </div>
              <div className="flex items-start">
                <span className="flex-shrink-0 w-6 h-6 rounded-full bg-ember-orange text-white flex items-center justify-center text-sm font-semibold mr-3">
                  4
                </span>
                <p className="text-gray-600">
                  Save your backup codes in case you lose access to your authenticator app
                </p>
              </div>
            </div>
          </div>

          {/* Actions */}
          <div className="space-y-4">
            {!totpEnabled ? (
              <Button fullWidth onClick={() => setStep('enroll')}>
                Enable Two-Factor Authentication
              </Button>
            ) : (
              <>
                <Button fullWidth variant="outline" onClick={handleRegenerateBackupCodes}>
                  Regenerate Backup Codes
                </Button>
                <Button
                  fullWidth
                  variant="danger"
                  onClick={handleDisable2FA}
                  isLoading={isDisabling}
                >
                  Disable Two-Factor Authentication
                </Button>
              </>
            )}

            <Button fullWidth variant="outline" onClick={() => navigate('/')}>
              Back to Dashboard
            </Button>
          </div>
        </div>
      </div>
    </div>
  )
}
