# Week 4 Day 3: Error Handling, Toast Notifications, and Loading States

**Date:** 2025-11-15
**Author:** Heinstein F.
**Status:** ✅ Completed

## Overview

Implemented comprehensive error handling, toast notification system, error boundaries, and loading states across the application to provide better user feedback and improve the overall UX.

---

## Components Implemented

### 1. Toast Notification System

#### `/ui/src/components/ui/Toast.tsx`
**Purpose:** Individual toast notification component with auto-dismiss functionality

**Features:**
- 4 toast types: `success`, `error`, `warning`, `info`
- Auto-dismiss with configurable duration (default: 5000ms)
- Manual close button
- Color-coded styling with icons from lucide-react
- Entry animation (slide-in from right)
- Accessible with ARIA labels

**Toast Types:**
- **Success** - Green with CheckCircle icon
- **Error** - Red with AlertCircle icon
- **Warning** - Yellow with AlertTriangle icon
- **Info** - Blue with Info icon

**Props:**
```typescript
interface ToastProps {
  id: string;
  type: ToastType;
  title: string;
  message?: string;
  duration?: number;
  onClose: (id: string) => void;
}
```

---

#### `/ui/src/contexts/ToastContext.tsx`
**Purpose:** Global toast management with React Context

**Features:**
- Centralized toast state management
- Toast provider wrapping the entire app
- Custom `useToast()` hook for easy access
- Helper methods for each toast type
- ToastContainer component for rendering toasts

**API:**
```typescript
const { success, error, warning, info, showToast, removeToast } = useToast();

// Usage examples:
success('Upload successful', 'File uploaded to S3');
error('Login failed', 'Invalid credentials');
warning('Quota warning', 'You are using 90% of your storage');
info('New feature', 'Check out our new dashboard');
```

**Integration:**
- Added to `App.tsx` wrapping all routes
- Available throughout the application via `useToast()` hook

---

### 2. Error Boundary

#### `/ui/src/components/ui/ErrorBoundary.tsx`
**Purpose:** Catch React errors and display user-friendly fallback UI

**Features:**
- Catches JavaScript errors anywhere in the component tree
- Displays friendly error message with recovery options
- Shows detailed error info in development mode
- "Try Again" button to reset error state
- "Go Home" button to navigate to homepage
- Logs errors to console (production: send to error reporting service)

**Error State:**
```typescript
interface State {
  hasError: boolean;
  error: Error | null;
  errorInfo: ErrorInfo | null;
}
```

**Fallback UI:**
- Alert icon with error message
- Development mode: Expandable error details with stack trace
- Production mode: Simple user-friendly message
- Recovery actions (Try Again, Go Home)

**Integration:**
- Wraps the entire application in `App.tsx`
- Catches errors from any child component

---

### 3. Loading Components

#### `/ui/src/components/ui/Loading.tsx`
**Purpose:** Reusable loading indicators and skeleton screens

**Components:**

**1. Spinner**
```typescript
<Spinner size="sm" | "md" | "lg" />
```
- Animated loader icon
- Three sizes: sm (16px), md (32px), lg (48px)
- Blue color matching brand

**2. Loading**
```typescript
<Loading
  message="Loading data..."
  fullScreen={true}
  size="md"
/>
```
- Spinner with optional message
- Full-screen overlay option
- Configurable spinner size

**3. Skeleton**
```typescript
<Skeleton
  variant="text" | "circular" | "rectangular"
  width={200}
  height={20}
/>
```
- Placeholder for loading content
- Pulse animation
- Three variants for different use cases

**4. SkeletonText**
```typescript
<SkeletonText lines={3} />
```
- Multiple skeleton lines for text placeholders
- Last line 80% width for natural appearance

**5. SkeletonCard**
```typescript
<SkeletonCard />
```
- Card-shaped skeleton with title and text lines
- Matches typical card component structure

**6. SkeletonTable**
```typescript
<SkeletonTable rows={5} columns={4} />
```
- Table skeleton with header and rows
- Configurable row/column count

---

## Enhanced Components with Toast Notifications

### 1. FileUpload Component
**File:** `/ui/src/components/upload/FileUpload.tsx`

**Toast Integration:**
- ✅ Success toast on upload completion
- ✅ Error toast on upload failure (with detailed message)
- ✅ Error toast on quota exceeded
- ✅ Error toast on quota check failure

**Error Handling Improvements:**
- Better error message extraction from API responses
- Automatic multipart upload abort on failure
- Quota check with user-friendly error messages

**Code Example:**
```typescript
// Success
success('Upload successful', `${file.name} uploaded successfully`);

// Error
const errorMessage = error.response?.data?.detail || error.message || 'Upload failed';
showErrorToast('Upload failed', `${file.name}: ${errorMessage}`);

// Quota exceeded
showErrorToast('Quota exceeded', errorMsg);
```

---

### 2. LoginPage Component
**File:** `/ui/src/pages/LoginPage.tsx`

**Toast Integration:**
- ✅ Success toast on successful login
- ✅ Success toast on 2FA verification
- ✅ Error toast on login failure
- ✅ Error toast on OAuth failure

**Code Example:**
```typescript
// Login success
success('Login successful', 'Welcome back!');

// 2FA verified
success('2FA verified', 'Authentication successful!');

// Login error
showErrorToast('Login failed', errorMessage);

// OAuth error
showErrorToast('OAuth failed', errorMessage);
```

---

## Application Structure Updates

### Updated `App.tsx`

**Before:**
```typescript
function App() {
  return (
    <Routes>
      {/* Routes */}
    </Routes>
  )
}
```

**After:**
```typescript
function App() {
  return (
    <ErrorBoundary>
      <ToastProvider>
        <Routes>
          {/* Routes */}
        </Routes>
      </ToastProvider>
    </ErrorBoundary>
  )
}
```

**Provider Order:**
1. **ErrorBoundary** (outermost) - Catches all React errors
2. **ToastProvider** - Provides toast context
3. **Routes** - Application routing

---

## Toast Notification Patterns

### 1. Success Notifications
**Use cases:**
- Upload completed
- Login successful
- 2FA verified
- Subscription created
- Payment processed
- Profile updated

**Pattern:**
```typescript
success('Action completed', 'Optional detailed message');
```

---

### 2. Error Notifications
**Use cases:**
- Upload failed
- Login failed
- Quota exceeded
- Payment failed
- Network error
- Validation error

**Pattern:**
```typescript
const errorMessage = err.response?.data?.detail || err.message || 'Operation failed';
showErrorToast('Error title', errorMessage);
```

---

### 3. Warning Notifications
**Use cases:**
- Quota approaching limit (>75%)
- Session expiring soon
- Incomplete form
- Unsaved changes

**Pattern:**
```typescript
warning('Warning title', 'Optional warning message');
```

---

### 4. Info Notifications
**Use cases:**
- New feature announcement
- Scheduled maintenance
- Tips and hints
- Non-critical updates

**Pattern:**
```typescript
info('Info title', 'Optional info message');
```

---

## Error Handling Best Practices

### 1. API Error Extraction
```typescript
const errorMessage =
  err.response?.data?.detail ||  // FastAPI validation error
  err.response?.data?.message ||  // Custom error message
  err.message ||                  // JavaScript error
  'Operation failed';             // Fallback
```

### 2. User-Friendly Messages
```typescript
// ❌ Bad
error('Error', err.message); // "Network Error" (not helpful)

// ✅ Good
error('Upload failed', 'Unable to connect to server. Please check your connection.');
```

### 3. Cleanup on Error
```typescript
try {
  // Operation
} catch (error) {
  showErrorToast('Operation failed', errorMessage);

  // Cleanup resources
  await abortMultipartUpload();
  resetFormState();
}
```

### 4. Loading States
```typescript
const [isLoading, setIsLoading] = useState(false);

try {
  setIsLoading(true);
  await operation();
  success('Success!');
} catch (error) {
  showErrorToast('Failed', error.message);
} finally {
  setIsLoading(false); // Always reset loading state
}
```

---

## Testing Recommendations

### Unit Tests

**Toast Component:**
```typescript
test('Toast auto-dismisses after duration', async () => {
  const onClose = jest.fn();
  render(<Toast id="1" type="success" title="Test" duration={100} onClose={onClose} />);
  await waitFor(() => expect(onClose).toHaveBeenCalledWith("1"), { timeout: 200 });
});
```

**ErrorBoundary:**
```typescript
test('ErrorBoundary catches errors and displays fallback', () => {
  const ThrowError = () => { throw new Error('Test error'); };
  render(
    <ErrorBoundary>
      <ThrowError />
    </ErrorBoundary>
  );
  expect(screen.getByText(/something went wrong/i)).toBeInTheDocument();
});
```

---

### Integration Tests

**Toast Integration:**
```typescript
test('Shows success toast on file upload', async () => {
  render(<FileUpload />);
  const file = new File(['content'], 'test.txt', { type: 'text/plain' });
  await uploadFile(file);
  expect(screen.getByText('Upload successful')).toBeInTheDocument();
});
```

**Error Handling:**
```typescript
test('Shows error toast on upload failure', async () => {
  server.use(
    http.post('/v1/uploads/presign', () => {
      return HttpResponse.json({ detail: 'Quota exceeded' }, { status: 413 });
    })
  );
  render(<FileUpload />);
  await uploadFile(file);
  expect(screen.getByText('Quota exceeded')).toBeInTheDocument();
});
```

---

## Future Enhancements

### 1. Toast Queue Management
- Limit maximum visible toasts (e.g., 3)
- Queue overflow toasts
- Stack toasts vertically with transitions

### 2. Persistent Toasts
```typescript
// Toast that requires manual dismissal
showToast({ type: 'error', title: 'Critical error', message: '...', duration: 0 });
```

### 3. Action Buttons in Toasts
```typescript
showToast({
  type: 'info',
  title: 'Update available',
  message: 'A new version is ready',
  action: { label: 'Reload', onClick: () => window.location.reload() }
});
```

### 4. Toast Positioning
- Support multiple positions (top-left, top-center, bottom-right, etc.)
- Per-toast positioning override

### 5. Error Reporting Integration
```typescript
// In ErrorBoundary.componentDidCatch()
if (process.env.NODE_ENV === 'production') {
  Sentry.captureException(error, { extra: errorInfo });
}
```

---

## Files Modified

### New Files Created:
1. `/ui/src/components/ui/Toast.tsx` (120 lines)
2. `/ui/src/contexts/ToastContext.tsx` (90 lines)
3. `/ui/src/components/ui/ErrorBoundary.tsx` (120 lines)
4. `/ui/src/components/ui/Loading.tsx` (130 lines)

### Files Modified:
1. `/ui/src/App.tsx` - Added ErrorBoundary and ToastProvider
2. `/ui/src/components/upload/FileUpload.tsx` - Added toast notifications and better error handling
3. `/ui/src/pages/LoginPage.tsx` - Added toast notifications

---

## Summary

Week 4 Day 3 successfully implemented a comprehensive error handling and notification system:

✅ **Toast notification system** - Success, error, warning, and info toasts
✅ **Error boundary** - Catches React errors with fallback UI
✅ **Loading components** - Spinners, skeletons for better UX
✅ **Enhanced FileUpload** - Toast notifications for all operations
✅ **Enhanced LoginPage** - Toast notifications for auth flows
✅ **Global error handling** - Consistent error extraction and display

**Next Steps:**
- Week 4 Day 4: Write integration tests covering error scenarios
- Week 4 Day 5: Complete documentation and prepare demo deployment

---

**Implementation Time:** ~3 hours
**Lines of Code:** ~460 new lines
**Components:** 4 new components, 3 enhanced components
**Status:** ✅ Ready for testing
