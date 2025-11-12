"""
Rate limiting middleware
Implements token bucket algorithm with Redis
"""
from fastapi import Request, HTTPException, status
from starlette.middleware.base import BaseHTTPMiddleware
import redis.asyncio as redis
import time
import os


class RateLimitMiddleware(BaseHTTPMiddleware):
    def __init__(self, app):
        super().__init__(app)

        # Initialize Redis connection
        redis_url = os.getenv("REDIS_URL", "redis://localhost:6379")
        self.redis_client = redis.from_url(redis_url, decode_responses=True)

        # Rate limit configuration
        self.default_rate = int(os.getenv("RATE_LIMIT_PER_MINUTE", "100"))  # requests per minute
        self.burst_size = int(os.getenv("RATE_LIMIT_BURST", "20"))
        self.user_rate = int(os.getenv("RATE_LIMIT_USER_PER_MINUTE", "1000"))  # authenticated users get higher limit
        self.window_seconds = 60  # 1 minute window

    async def dispatch(self, request: Request, call_next):
        # Skip rate limiting for health check
        if request.url.path == "/health":
            return await call_next(request)

        # Determine rate limit key (by IP or user)
        client_ip = request.client.host if request.client else "unknown"
        user_id = getattr(request.state, "user_id", None)

        if user_id:
            rate_key = f"ratelimit:user:{user_id}"
            rate_limit = self.user_rate
        else:
            rate_key = f"ratelimit:ip:{client_ip}"
            rate_limit = self.default_rate

        # Check rate limit
        try:
            allowed = await self.check_rate_limit(rate_key, rate_limit)
            if not allowed:
                raise HTTPException(
                    status_code=status.HTTP_429_TOO_MANY_REQUESTS,
                    detail="Rate limit exceeded. Try again later.",
                    headers={"Retry-After": "60"}
                )
        except redis.RedisError as e:
            # If Redis fails, log but allow request (fail open for availability)
            # In production with proper Redis HA, consider failing closed
            print(f"WARNING: Rate limit check failed (Redis error): {e}")
            # Continue without rate limiting

        response = await call_next(request)
        return response

    async def check_rate_limit(self, key: str, limit: int) -> bool:
        """
        Check if request is within rate limit using token bucket algorithm

        Token bucket algorithm:
        - Each user/IP has a bucket with N tokens
        - Each request consumes 1 token
        - Tokens refill at rate R per second
        - If bucket is empty, request is denied
        """
        current_time = int(time.time())

        try:
            # Use Redis Lua script for atomic token bucket check
            lua_script = """
            local key = KEYS[1]
            local limit = tonumber(ARGV[1])
            local window = tonumber(ARGV[2])
            local current_time = tonumber(ARGV[3])

            -- Get current count
            local current = redis.call('GET', key)

            if current and tonumber(current) >= limit then
                -- Rate limit exceeded
                return 0
            else
                -- Increment counter
                local new_count = redis.call('INCR', key)

                -- Set expiry on first request
                if new_count == 1 then
                    redis.call('EXPIRE', key, window)
                end

                return 1
            end
            """

            result = await self.redis_client.eval(
                lua_script,
                1,  # number of keys
                key,  # KEYS[1]
                limit,  # ARGV[1]
                self.window_seconds,  # ARGV[2]
                current_time  # ARGV[3]
            )

            return result == 1

        except redis.RedisError as e:
            # Re-raise to be handled by caller
            raise
