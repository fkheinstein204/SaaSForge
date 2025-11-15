/**
 * @file        UploadsPage.tsx
 * @brief       File uploads management page with storage usage
 * @author      Heinstein F.
 * @date        2025-11-15
 */

import React, { useState, useEffect } from 'react';
import { FileUpload } from '../components/upload/FileUpload';
import { Alert } from '../components/ui/Alert';
import { HardDrive, Upload, Download, Trash2 } from 'lucide-react';
import { apiClient } from '../api/client';

interface StorageUsage {
  total_size: number;
  object_count: number;
  quota_bytes: number;
  quota_percentage: number;
}

export function UploadsPage() {
  const [storageUsage, setStorageUsage] = useState<StorageUsage | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [successMessage, setSuccessMessage] = useState<string | null>(null);

  useEffect(() => {
    fetchStorageUsage();
  }, []);

  const fetchStorageUsage = async () => {
    try {
      const response = await apiClient.get('/v1/uploads/storage/usage');
      setStorageUsage(response.data);
    } catch (err: any) {
      console.error('Failed to fetch storage usage:', err);
      setError('Failed to load storage usage');
    }
  };

  const handleUploadComplete = (objectKey: string, fileName: string) => {
    setSuccessMessage(`Successfully uploaded: ${fileName}`);
    fetchStorageUsage(); // Refresh storage usage

    // Clear success message after 5 seconds
    setTimeout(() => setSuccessMessage(null), 5000);
  };

  const formatBytes = (bytes: number): string => {
    if (bytes === 0) return '0 Bytes';
    const k = 1024;
    const sizes = ['Bytes', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return Math.round((bytes / Math.pow(k, i)) * 100) / 100 + ' ' + sizes[i];
  };

  const getStorageColor = (percentage: number): string => {
    if (percentage >= 90) return 'bg-red-600';
    if (percentage >= 75) return 'bg-yellow-600';
    return 'bg-blue-600';
  };

  return (
    <div className="max-w-4xl mx-auto">
      <div className="mb-8">
        <h1 className="text-3xl font-bold text-gray-900">File Uploads</h1>
        <p className="mt-2 text-gray-600">
          Upload and manage your files with drag-and-drop support
        </p>
      </div>

      {/* Alerts */}
      {error && (
        <Alert type="error" onClose={() => setError(null)} className="mb-4">
          {error}
        </Alert>
      )}

      {successMessage && (
        <Alert type="success" onClose={() => setSuccessMessage(null)} className="mb-4">
          {successMessage}
        </Alert>
      )}

      {/* Storage Usage Card */}
      {storageUsage && (
        <div className="bg-white rounded-lg shadow p-6 mb-6">
          <div className="flex items-center justify-between mb-4">
            <div className="flex items-center gap-3">
              <HardDrive className="h-6 w-6 text-gray-600" />
              <h2 className="text-lg font-semibold text-gray-900">Storage Usage</h2>
            </div>
            <div className="text-sm text-gray-600">
              {storageUsage.object_count} file{storageUsage.object_count !== 1 ? 's' : ''}
            </div>
          </div>

          <div className="space-y-3">
            <div className="flex justify-between text-sm">
              <span className="text-gray-600">
                {formatBytes(storageUsage.total_size)} of {formatBytes(storageUsage.quota_bytes)} used
              </span>
              <span className="font-medium text-gray-900">
                {storageUsage.quota_percentage.toFixed(1)}%
              </span>
            </div>

            <div className="w-full bg-gray-200 rounded-full h-2.5">
              <div
                className={`h-2.5 rounded-full transition-all ${getStorageColor(
                  storageUsage.quota_percentage
                )}`}
                style={{ width: `${Math.min(storageUsage.quota_percentage, 100)}%` }}
              />
            </div>

            {storageUsage.quota_percentage >= 90 && (
              <div className="text-sm text-red-600 font-medium">
                ⚠️ Storage almost full. Please delete some files or upgrade your plan.
              </div>
            )}
          </div>
        </div>
      )}

      {/* Upload Section */}
      <div className="bg-white rounded-lg shadow p-6 mb-6">
        <div className="flex items-center gap-3 mb-4">
          <Upload className="h-6 w-6 text-gray-600" />
          <h2 className="text-lg font-semibold text-gray-900">Upload Files</h2>
        </div>

        <FileUpload
          onUploadComplete={handleUploadComplete}
          folder="uploads"
          multiple={true}
        />
      </div>

      {/* Info Cards */}
      <div className="grid grid-cols-1 md:grid-cols-2 gap-6">
        <div className="bg-blue-50 border border-blue-200 rounded-lg p-4">
          <h3 className="text-sm font-semibold text-blue-900 mb-2">
            Small Files (&lt;100MB)
          </h3>
          <p className="text-sm text-blue-700">
            Files under 100MB are uploaded directly with a single presigned URL.
            Fast and efficient for most documents and images.
          </p>
        </div>

        <div className="bg-green-50 border border-green-200 rounded-lg p-4">
          <h3 className="text-sm font-semibold text-green-900 mb-2">
            Large Files (&gt;100MB)
          </h3>
          <p className="text-sm text-green-700">
            Large files use multipart upload, splitting into chunks for reliable
            transmission. Supports files up to 5GB.
          </p>
        </div>
      </div>

      {/* Features List */}
      <div className="mt-6 bg-gray-50 border border-gray-200 rounded-lg p-6">
        <h3 className="text-sm font-semibold text-gray-900 mb-3">Features</h3>
        <ul className="space-y-2 text-sm text-gray-700">
          <li className="flex items-start gap-2">
            <span className="text-green-600 mt-0.5">✓</span>
            <span>Drag-and-drop file upload</span>
          </li>
          <li className="flex items-start gap-2">
            <span className="text-green-600 mt-0.5">✓</span>
            <span>Progress tracking for all uploads</span>
          </li>
          <li className="flex items-start gap-2">
            <span className="text-green-600 mt-0.5">✓</span>
            <span>SHA-256 checksum validation</span>
          </li>
          <li className="flex items-start gap-2">
            <span className="text-green-600 mt-0.5">✓</span>
            <span>Automatic multipart upload for large files</span>
          </li>
          <li className="flex items-start gap-2">
            <span className="text-green-600 mt-0.5">✓</span>
            <span>Storage quota enforcement</span>
          </li>
          <li className="flex items-start gap-2">
            <span className="text-green-600 mt-0.5">✓</span>
            <span>Tenant and user isolation</span>
          </li>
        </ul>
      </div>
    </div>
  );
}
