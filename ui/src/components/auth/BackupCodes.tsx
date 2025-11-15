/**
 * @file        BackupCodes.tsx
 * @brief       Backup codes display and download component
 *
 * @copyright   (c) 2025 FtsCoDe GmbH. All rights reserved.
 * @author      Heinstein F.
 * @date        2025-11-15
 */

import { useState } from 'react'
import { Button } from '@/components/ui/Button'
import { Alert } from '@/components/ui/Alert'

interface BackupCodesProps {
  codes: string[]
  onComplete: () => void
}

export function BackupCodes({ codes, onComplete }: BackupCodesProps) {
  const [downloaded, setDownloaded] = useState(false)
  const [copied, setCopied] = useState(false)

  const handleDownload = () => {
    const content = `SaaSForge Backup Codes
Generated: ${new Date().toLocaleString()}

IMPORTANT: Store these codes in a safe place. Each code can only be used once.

${codes.map((code, i) => `${i + 1}. ${code}`).join('\n')}

If you lose access to your authenticator app, you can use these codes to sign in.
`

    const blob = new Blob([content], { type: 'text/plain' })
    const url = URL.createObjectURL(blob)
    const a = document.createElement('a')
    a.href = url
    a.download = `saasforge-backup-codes-${Date.now()}.txt`
    document.body.appendChild(a)
    a.click()
    document.body.removeChild(a)
    URL.revokeObjectURL(url)
    setDownloaded(true)
  }

  const handleCopy = () => {
    const text = codes.join('\n')
    navigator.clipboard.writeText(text)
    setCopied(true)
    setTimeout(() => setCopied(false), 2000)
  }

  const handlePrint = () => {
    const printWindow = window.open('', '_blank')
    if (printWindow) {
      printWindow.document.write(`
        <html>
          <head>
            <title>SaaSForge Backup Codes</title>
            <style>
              body { font-family: monospace; padding: 40px; }
              h1 { color: #0A2540; }
              .codes { margin: 20px 0; }
              .code { margin: 10px 0; padding: 10px; background: #f0f0f0; }
              .warning { color: #d32f2f; font-weight: bold; margin: 20px 0; }
            </style>
          </head>
          <body>
            <h1>SaaSForge Backup Codes</h1>
            <p>Generated: ${new Date().toLocaleString()}</p>
            <div class="warning">⚠️ Store these codes in a safe place. Each code can only be used once.</div>
            <div class="codes">
              ${codes.map((code, i) => `<div class="code">${i + 1}. ${code}</div>`).join('')}
            </div>
            <p>If you lose access to your authenticator app, you can use these codes to sign in.</p>
          </body>
        </html>
      `)
      printWindow.document.close()
      printWindow.print()
    }
  }

  return (
    <div className="space-y-6">
      <Alert type="warning" title="Save Your Backup Codes">
        Store these codes in a safe place. You can use them to access your account if you lose your
        authenticator device. Each code can only be used once.
      </Alert>

      {/* Backup Codes Grid */}
      <div className="bg-gray-50 p-6 rounded-lg border-2 border-gray-200">
        <div className="grid grid-cols-2 gap-3">
          {codes.map((code, index) => (
            <div
              key={index}
              className="bg-white px-4 py-3 rounded border border-gray-300 font-mono text-center"
            >
              <span className="text-xs text-gray-500 mr-2">{index + 1}.</span>
              <span className="text-sm font-semibold">{code}</span>
            </div>
          ))}
        </div>
      </div>

      {/* Action Buttons */}
      <div className="space-y-3">
        <div className="grid grid-cols-3 gap-3">
          <Button variant="outline" onClick={handleDownload} fullWidth>
            {downloaded ? '✓ Downloaded' : 'Download'}
          </Button>
          <Button variant="outline" onClick={handleCopy} fullWidth>
            {copied ? '✓ Copied' : 'Copy'}
          </Button>
          <Button variant="outline" onClick={handlePrint} fullWidth>
            Print
          </Button>
        </div>

        <Button fullWidth onClick={onComplete} disabled={!downloaded}>
          {downloaded ? 'Continue' : 'Download codes to continue'}
        </Button>
      </div>

      {!downloaded && (
        <p className="text-sm text-center text-gray-600">
          You must download your backup codes before continuing
        </p>
      )}
    </div>
  )
}
