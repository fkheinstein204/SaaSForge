/**
 * @file        FileUpload.tsx
 * @brief       File upload component with drag-and-drop, progress tracking, and multipart support
 * @author      Heinstein F.
 * @date        2025-11-15
 */

import React, { useState, useRef, useCallback } from 'react';
import { Upload, X, File, CheckCircle, AlertCircle } from 'lucide-react';
import { Button } from '../ui/Button';
import { Alert } from '../ui/Alert';
import { apiClient } from '../../api/client';
import { useToast } from '../../contexts/ToastContext';

interface FileUploadProps {
  onUploadComplete?: (objectKey: string, fileName: string) => void;
  folder?: 'uploads' | 'avatars' | 'documents' | 'temp';
  accept?: string;
  maxSize?: number; // in bytes
  multiple?: boolean;
}

interface UploadingFile {
  id: string;
  file: File;
  progress: number;
  status: 'pending' | 'uploading' | 'completed' | 'error';
  error?: string;
  objectKey?: string;
}

export function FileUpload({
  onUploadComplete,
  folder = 'uploads',
  accept = '*/*',
  maxSize = 5 * 1024 * 1024 * 1024, // 5GB default
  multiple = true,
}: FileUploadProps) {
  const [files, setFiles] = useState<UploadingFile[]>([]);
  const [isDragging, setIsDragging] = useState(false);
  const [quotaError, setQuotaError] = useState<string | null>(null);
  const fileInputRef = useRef<HTMLInputElement>(null);
  const { success, error: showErrorToast } = useToast();

  const MULTIPART_THRESHOLD = 100 * 1024 * 1024; // 100MB

  const calculateSHA256 = async (file: File): Promise<string> => {
    const buffer = await file.arrayBuffer();
    const hashBuffer = await crypto.subtle.digest('SHA-256', buffer);
    const hashArray = Array.from(new Uint8Array(hashBuffer));
    return hashArray.map(b => b.toString(16).padStart(2, '0')).join('');
  };

  const uploadSmallFile = async (uploadFile: UploadingFile) => {
    const { file } = uploadFile;

    try {
      // Calculate checksum
      const checksum = await calculateSHA256(file);

      // Request presigned URL
      const presignResponse = await apiClient.post('/v1/uploads/presign', {
        filename: file.name,
        content_type: file.type || 'application/octet-stream',
        content_length: file.size,
        checksum_sha256: checksum,
        folder,
      });

      const { presigned_url, object_key, headers } = presignResponse.data;

      // Upload to S3
      const uploadResponse = await fetch(presigned_url, {
        method: 'PUT',
        body: file,
        headers: {
          'Content-Type': headers['Content-Type'],
          'Content-Length': headers['Content-Length'],
        },
      });

      if (!uploadResponse.ok) {
        throw new Error(`Upload failed: ${uploadResponse.statusText}`);
      }

      // Update file status
      setFiles(prev =>
        prev.map(f =>
          f.id === uploadFile.id
            ? { ...f, status: 'completed', progress: 100, objectKey: object_key }
            : f
        )
      );

      success('Upload successful', `${file.name} uploaded successfully`);
      onUploadComplete?.(object_key, file.name);
    } catch (error: any) {
      const errorMessage = error.response?.data?.detail || error.message || 'Upload failed';
      setFiles(prev =>
        prev.map(f =>
          f.id === uploadFile.id
            ? { ...f, status: 'error', error: errorMessage }
            : f
        )
      );
      showErrorToast('Upload failed', `${file.name}: ${errorMessage}`);
    }
  };

  const uploadLargeFile = async (uploadFile: UploadingFile) => {
    const { file } = uploadFile;

    try {
      // Calculate checksum
      const checksum = await calculateSHA256(file);

      // Initiate multipart upload
      const initResponse = await apiClient.post('/v1/uploads/multipart/init', {
        filename: file.name,
        content_type: file.type || 'application/octet-stream',
        total_size: file.size,
        checksum_sha256: checksum,
        folder,
      });

      const { upload_id, object_key, part_size, num_parts } = initResponse.data;

      // Get presigned URLs for parts
      const partsResponse = await apiClient.get(
        `/v1/uploads/multipart/${upload_id}/parts`,
        {
          params: { object_key, num_parts },
        }
      );

      const { parts } = partsResponse.data;

      // Upload parts
      const uploadedParts: { part_number: number; etag: string }[] = [];

      for (let i = 0; i < parts.length; i++) {
        const part = parts[i];
        const start = i * part_size;
        const end = Math.min(start + part_size, file.size);
        const chunk = file.slice(start, end);

        const response = await fetch(part.presigned_url, {
          method: 'PUT',
          body: chunk,
        });

        if (!response.ok) {
          throw new Error(`Part ${i + 1} upload failed: ${response.statusText}`);
        }

        const etag = response.headers.get('ETag')?.replace(/"/g, '') || '';
        uploadedParts.push({
          part_number: part.part_number,
          etag,
        });

        // Update progress
        const progress = Math.round(((i + 1) / parts.length) * 100);
        setFiles(prev =>
          prev.map(f =>
            f.id === uploadFile.id ? { ...f, progress, status: 'uploading' } : f
          )
        );
      }

      // Complete multipart upload
      await apiClient.post('/v1/uploads/multipart/complete', {
        upload_id,
        object_key,
        parts: uploadedParts,
      });

      // Update file status
      setFiles(prev =>
        prev.map(f =>
          f.id === uploadFile.id
            ? { ...f, status: 'completed', progress: 100, objectKey: object_key }
            : f
        )
      );

      success('Upload successful', `${file.name} uploaded successfully`);
      onUploadComplete?.(object_key, file.name);
    } catch (error: any) {
      const errorMessage = error.response?.data?.detail || error.message || 'Upload failed';
      setFiles(prev =>
        prev.map(f =>
          f.id === uploadFile.id
            ? { ...f, status: 'error', error: errorMessage }
            : f
        )
      );
      showErrorToast('Upload failed', `${file.name}: ${errorMessage}`);

      // Attempt to abort the multipart upload
      try {
        await apiClient.post('/v1/uploads/multipart/abort', {
          upload_id,
          object_key,
        });
      } catch (abortError) {
        console.error('Failed to abort multipart upload:', abortError);
      }
    }
  };

  const uploadFile = async (uploadFile: UploadingFile) => {
    const { file } = uploadFile;

    // Update status to uploading
    setFiles(prev =>
      prev.map(f => (f.id === uploadFile.id ? { ...f, status: 'uploading' } : f))
    );

    // Check quota before upload
    try {
      const quotaResponse = await apiClient.post('/v1/uploads/quota/check', {
        file_size: file.size,
      });

      if (!quotaResponse.data.allowed) {
        const errorMsg = `Storage quota exceeded. You have ${formatBytes(
          quotaResponse.data.available_bytes
        )} available, but need ${formatBytes(file.size)}.`;
        setQuotaError(errorMsg);
        setFiles(prev =>
          prev.map(f =>
            f.id === uploadFile.id
              ? { ...f, status: 'error', error: 'Quota exceeded' }
              : f
          )
        );
        showErrorToast('Quota exceeded', errorMsg);
        return;
      }
    } catch (error: any) {
      console.error('Quota check failed:', error);
      showErrorToast('Quota check failed', 'Unable to verify storage quota');
    }

    // Choose upload method based on file size
    if (file.size > MULTIPART_THRESHOLD) {
      await uploadLargeFile(uploadFile);
    } else {
      await uploadSmallFile(uploadFile);
    }
  };

  const handleFiles = useCallback(
    (fileList: FileList | null) => {
      if (!fileList || fileList.length === 0) return;

      setQuotaError(null);

      const newFiles: UploadingFile[] = Array.from(fileList).map(file => ({
        id: `${Date.now()}-${Math.random()}`,
        file,
        progress: 0,
        status: 'pending',
      }));

      // Validate file sizes
      const oversizedFiles = newFiles.filter(f => f.file.size > maxSize);
      if (oversizedFiles.length > 0) {
        setQuotaError(
          `Some files exceed the maximum size of ${formatBytes(maxSize)}`
        );
        return;
      }

      setFiles(prev => [...prev, ...newFiles]);

      // Start uploading
      newFiles.forEach(uploadFile);
    },
    [maxSize, folder]
  );

  const handleDragOver = (e: React.DragEvent) => {
    e.preventDefault();
    setIsDragging(true);
  };

  const handleDragLeave = () => {
    setIsDragging(false);
  };

  const handleDrop = (e: React.DragEvent) => {
    e.preventDefault();
    setIsDragging(false);
    handleFiles(e.dataTransfer.files);
  };

  const handleFileInputChange = (e: React.ChangeEvent<HTMLInputElement>) => {
    handleFiles(e.target.files);
    if (fileInputRef.current) {
      fileInputRef.current.value = '';
    }
  };

  const removeFile = (id: string) => {
    setFiles(prev => prev.filter(f => f.id !== id));
  };

  const formatBytes = (bytes: number): string => {
    if (bytes === 0) return '0 Bytes';
    const k = 1024;
    const sizes = ['Bytes', 'KB', 'MB', 'GB'];
    const i = Math.floor(Math.log(bytes) / Math.log(k));
    return Math.round((bytes / Math.pow(k, i)) * 100) / 100 + ' ' + sizes[i];
  };

  return (
    <div className="space-y-4">
      {/* Drop zone */}
      <div
        onDragOver={handleDragOver}
        onDragLeave={handleDragLeave}
        onDrop={handleDrop}
        className={`
          border-2 border-dashed rounded-lg p-8 text-center transition-colors
          ${
            isDragging
              ? 'border-blue-500 bg-blue-50'
              : 'border-gray-300 hover:border-gray-400'
          }
        `}
      >
        <Upload className="mx-auto h-12 w-12 text-gray-400 mb-4" />
        <div className="text-sm text-gray-600 mb-2">
          <span className="font-medium text-blue-600 hover:text-blue-500 cursor-pointer">
            Click to upload
          </span>{' '}
          or drag and drop
        </div>
        <p className="text-xs text-gray-500">
          Max file size: {formatBytes(maxSize)}
        </p>

        <input
          ref={fileInputRef}
          type="file"
          className="hidden"
          accept={accept}
          multiple={multiple}
          onChange={handleFileInputChange}
        />

        <Button
          variant="primary"
          onClick={() => fileInputRef.current?.click()}
          className="mt-4"
        >
          Select Files
        </Button>
      </div>

      {/* Quota error */}
      {quotaError && (
        <Alert type="error" onClose={() => setQuotaError(null)}>
          {quotaError}
        </Alert>
      )}

      {/* File list */}
      {files.length > 0 && (
        <div className="space-y-2">
          {files.map(file => (
            <div
              key={file.id}
              className="flex items-center gap-3 p-3 bg-gray-50 rounded-lg"
            >
              <div className="flex-shrink-0">
                {file.status === 'completed' && (
                  <CheckCircle className="h-5 w-5 text-green-500" />
                )}
                {file.status === 'error' && (
                  <AlertCircle className="h-5 w-5 text-red-500" />
                )}
                {file.status === 'pending' && (
                  <File className="h-5 w-5 text-gray-400" />
                )}
                {file.status === 'uploading' && (
                  <div className="h-5 w-5 border-2 border-blue-500 border-t-transparent rounded-full animate-spin" />
                )}
              </div>

              <div className="flex-1 min-w-0">
                <div className="flex items-center justify-between mb-1">
                  <p className="text-sm font-medium text-gray-900 truncate">
                    {file.file.name}
                  </p>
                  <span className="text-xs text-gray-500 ml-2">
                    {formatBytes(file.file.size)}
                  </span>
                </div>

                {file.status === 'uploading' && (
                  <div className="w-full bg-gray-200 rounded-full h-1.5">
                    <div
                      className="bg-blue-600 h-1.5 rounded-full transition-all"
                      style={{ width: `${file.progress}%` }}
                    />
                  </div>
                )}

                {file.status === 'error' && (
                  <p className="text-xs text-red-600">{file.error}</p>
                )}

                {file.status === 'completed' && (
                  <p className="text-xs text-green-600">Upload complete</p>
                )}
              </div>

              <button
                onClick={() => removeFile(file.id)}
                className="flex-shrink-0 text-gray-400 hover:text-gray-600"
                aria-label="Remove file"
              >
                <X className="h-4 w-4" />
              </button>
            </div>
          ))}
        </div>
      )}
    </div>
  );
}
