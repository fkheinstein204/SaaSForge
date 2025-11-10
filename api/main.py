"""
FastAPI BFF (Backend for Frontend)
Main application entry point
"""
from fastapi import FastAPI, Request, status
from fastapi.middleware.cors import CORSMiddleware
from fastapi.middleware.trustedhost import TrustedHostMiddleware
from fastapi.responses import JSONResponse
from contextlib import asynccontextmanager
import uvicorn

from routers import auth, upload, payment, notification
from middleware.jwt_middleware import JWTMiddleware
from middleware.rate_limit_middleware import RateLimitMiddleware
from middleware.logging_middleware import LoggingMiddleware


@asynccontextmanager
async def lifespan(app: FastAPI):
    """Startup and shutdown events"""
    # Startup
    print("Starting FastAPI BFF on port 8000...")
    print("gRPC backends: auth:50051, upload:50052, payment:50053, notification:50054")
    yield
    # Shutdown
    print("Shutting down FastAPI BFF...")


app = FastAPI(
    title="SaaSForge API",
    description="Multi-tenant SaaS boilerplate API",
    version="0.1.0",
    lifespan=lifespan
)

# CORS middleware
app.add_middleware(
    CORSMiddleware,
    allow_origins=["http://localhost:3000"],  # React dev server
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Security middleware
app.add_middleware(TrustedHostMiddleware, allowed_hosts=["*"])  # Configure in production

# Custom middleware
app.add_middleware(LoggingMiddleware)
app.add_middleware(RateLimitMiddleware)
app.add_middleware(JWTMiddleware)

# Include routers
app.include_router(auth.router, prefix="/v1/auth", tags=["authentication"])
app.include_router(upload.router, prefix="/v1/uploads", tags=["uploads"])
app.include_router(payment.router, prefix="/v1/payments", tags=["payments"])
app.include_router(notification.router, prefix="/v1/notifications", tags=["notifications"])


@app.get("/health")
async def health_check():
    """Health check endpoint"""
    return {"status": "healthy", "service": "saasforge-api"}


@app.exception_handler(Exception)
async def global_exception_handler(request: Request, exc: Exception):
    """Global exception handler with standard error format"""
    return JSONResponse(
        status_code=status.HTTP_500_INTERNAL_SERVER_ERROR,
        content={
            "code": "INTERNAL_ERROR",
            "message": "An unexpected error occurred",
            "correlationId": request.state.correlation_id if hasattr(request.state, 'correlation_id') else None
        }
    )


if __name__ == "__main__":
    uvicorn.run("main:app", host="0.0.0.0", port=8000, reload=True)
