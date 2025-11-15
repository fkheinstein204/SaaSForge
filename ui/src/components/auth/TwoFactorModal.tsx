/**
 * @file        TwoFactorModal.tsx
 * @brief       Modal for 2FA verification during login
 *
 * @copyright   (c) 2025 FtsCoDe GmbH. All rights reserved.
 * @author      Heinstein F.
 * @date        2025-11-15
 */

import { useState } from 'react'
import { Input } from '@/components/ui/Input'
import { Button } from '@/components/ui/Button'
import { Alert } from '@/components/ui/Alert'

interface TwoFactorModalProps {
  onVerify: (code: string, useBackupCode: boolean) => Promise<void>
  onCancel: () => void
}

export function TwoFactorModal({ onVerify, onCancel }: TwoFactorModalProps) {
  const [code, setCode] = useState('')
  const [error, setError] = useState('')
  const [isLoading, setIsLoading] = useState(false)
  const [useBackupCode, setUseBackupCode] = useState(false)

  const handleSubmit = async (e: React.FormEvent) => {
    e.preventDefault()
    setError('')

    const expectedLength = useBackupCode ? 8 : 6
    if (!code || code.length !== expectedLength) {
      setError(`Please enter a valid ${expectedLength}-character code`)
      return
    }

    setIsLoading(true)

    try {
      await onVerify(code, useBackupCode)
    } catch (err: any) {
      setError(err.response?.data?.message || 'Invalid code. Please try again.')
    } finally {
      setIsLoading(false)
    }
  }

  return (
    <div className="fixed inset-0 bg-black bg-opacity-50 flex items-center justify-center z-50 p-4">
      <div className="bg-white rounded-lg shadow-xl max-w-md w-full p-6 space-y-6">
        <div className="text-center">
          <h2 className="text-2xl font-bold text-gray-900">Two-Factor Authentication</h2>
          <p className="mt-2 text-sm text-gray-600">
            {useBackupCode
              ? 'Enter one of your backup codes'
              : 'Enter the 6-digit code from your authenticator app'}
          </p>
        </div>

        {error && (
          <Alert type="error" onClose={() => setError('')}>
            {error}
          </Alert>
        )}

        <form onSubmit={handleSubmit} className="space-y-4">
          <Input
            label={useBackupCode ? 'Backup Code' : 'Verification Code'}
            type="text"
            inputMode={useBackupCode ? 'text' : 'numeric'}
            pattern={useBackupCode ? '[A-Z0-9]{8}' : '[0-9]{6}'}
            maxLength={useBackupCode ? 8 : 6}
            value={code}
            onChange={(e) =>
              setCode(useBackupCode ? e.target.value.toUpperCase() : e.target.value.replace(/\D/g, ''))
            }
            placeholder={useBackupCode ? 'XXXXXXXX' : '000000'}
            autoFocus
            disabled={isLoading}
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

        <div className="text-center">
          <button
            type="button"
            onClick={() => {
              setUseBackupCode(!useBackupCode)
              setCode('')
              setError('')
            }}
            className="text-sm text-ember-orange hover:text-opacity-80 font-medium"
          >
            {useBackupCode ? 'Use authenticator code instead' : 'Use backup code instead'}
          </button>
        </div>
      </div>
    </div>
  )
}
