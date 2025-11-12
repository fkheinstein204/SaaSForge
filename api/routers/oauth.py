"""OAuth 2.0/OIDC integration for Google, GitHub, Microsoft."""

from fastapi import APIRouter, Request, HTTPException, Depends
from fastapi.responses import RedirectResponse
from authlib.integrations.starlette_client import OAuth
from starlette.config import Config
import os
import secrets
from typing import Optional
import redis.asyncio as redis

from clients.auth_client import AuthClient

router = APIRouter(prefix="/oauth", tags=["oauth"])

# OAuth configuration
config = Config(environ={
    "GOOGLE_CLIENT_ID": os.getenv("GOOGLE_CLIENT_ID", ""),
    "GOOGLE_CLIENT_SECRET": os.getenv("GOOGLE_CLIENT_SECRET", ""),
    "GITHUB_CLIENT_ID": os.getenv("GITHUB_CLIENT_ID", ""),
    "GITHUB_CLIENT_SECRET": os.getenv("GITHUB_CLIENT_SECRET", ""),
    "MICROSOFT_CLIENT_ID": os.getenv("MICROSOFT_CLIENT_ID", ""),
    "MICROSOFT_CLIENT_SECRET": os.getenv("MICROSOFT_CLIENT_SECRET", ""),
})

oauth = OAuth(config)

# Register OAuth providers
oauth.register(
    name='google',
    client_id=config.get('GOOGLE_CLIENT_ID'),
    client_secret=config.get('GOOGLE_CLIENT_SECRET'),
    server_metadata_url='https://accounts.google.com/.well-known/openid-configuration',
    client_kwargs={
        'scope': 'openid email profile'
    }
)

oauth.register(
    name='github',
    client_id=config.get('GITHUB_CLIENT_ID'),
    client_secret=config.get('GITHUB_CLIENT_SECRET'),
    access_token_url='https://github.com/login/oauth/access_token',
    access_token_params=None,
    authorize_url='https://github.com/login/oauth/authorize',
    authorize_params=None,
    api_base_url='https://api.github.com/',
    client_kwargs={'scope': 'user:email'},
)

oauth.register(
    name='microsoft',
    client_id=config.get('MICROSOFT_CLIENT_ID'),
    client_secret=config.get('MICROSOFT_CLIENT_SECRET'),
    server_metadata_url='https://login.microsoftonline.com/common/v2.0/.well-known/openid-configuration',
    client_kwargs={
        'scope': 'openid email profile'
    }
)

# Redis for state storage
redis_client = None

async def get_redis():
    """Get Redis client for state storage."""
    global redis_client
    if redis_client is None:
        redis_url = os.getenv("REDIS_URL", "redis://localhost:6379")
        redis_client = await redis.from_url(redis_url, decode_responses=True)
    return redis_client


@router.get("/login/{provider}")
async def oauth_login(
    provider: str,
    request: Request,
    redirect_uri: Optional[str] = None
):
    """
    Initiate OAuth login flow for specified provider.

    Supported providers: google, github, microsoft
    """
    if provider not in ['google', 'github', 'microsoft']:
        raise HTTPException(status_code=400, detail="Unsupported OAuth provider")

    # Generate and store state for CSRF protection
    state = secrets.token_urlsafe(32)
    redis_conn = await get_redis()
    await redis_conn.setex(f"oauth:state:{state}", 600, redirect_uri or "/")  # 10 min expiry

    # Get callback URL
    callback_url = request.url_for('oauth_callback', provider=provider)

    # Redirect to OAuth provider
    client = oauth.create_client(provider)
    return await client.authorize_redirect(request, callback_url, state=state)


@router.get("/callback/{provider}")
async def oauth_callback(
    provider: str,
    request: Request,
    state: str,
    code: Optional[str] = None,
    error: Optional[str] = None
):
    """
    Handle OAuth callback from provider.

    Requirements:
    - A-34: Validate state parameter to prevent CSRF
    - A-35: Exchange code for tokens and verify signatures
    - A-36: Link accounts via email matching
    """
    if error:
        # A-39: Handle OAuth errors gracefully
        raise HTTPException(
            status_code=400,
            detail=f"OAuth error: {error}. Please try again."
        )

    # A-34: Validate state to prevent CSRF
    redis_conn = await get_redis()
    redirect_uri = await redis_conn.get(f"oauth:state:{state}")

    if not redirect_uri:
        raise HTTPException(
            status_code=400,
            detail="Invalid or expired OAuth state. Please try again."
        )

    # Delete state after use (one-time use)
    await redis_conn.delete(f"oauth:state:{state}")

    # A-35: Exchange authorization code for tokens
    client = oauth.create_client(provider)
    try:
        token = await client.authorize_access_token(request)
    except Exception as e:
        raise HTTPException(
            status_code=400,
            detail=f"Failed to exchange authorization code: {str(e)}"
        )

    # Get user info from provider
    if provider == 'google':
        user_info = token.get('userinfo')
        if not user_info:
            resp = await client.get('https://www.googleapis.com/oauth2/v3/userinfo')
            user_info = resp.json()
        email = user_info.get('email')
        name = user_info.get('name')
        provider_user_id = user_info.get('sub')

    elif provider == 'github':
        resp = await client.get('user')
        user_info = resp.json()
        email = user_info.get('email')

        # If email is None, get primary email from GitHub API
        if not email:
            emails_resp = await client.get('user/emails')
            emails = emails_resp.json()
            for e in emails:
                if e.get('primary'):
                    email = e.get('email')
                    break

        name = user_info.get('name') or user_info.get('login')
        provider_user_id = str(user_info.get('id'))

    elif provider == 'microsoft':
        resp = await client.get('https://graph.microsoft.com/v1.0/me')
        user_info = resp.json()
        email = user_info.get('mail') or user_info.get('userPrincipalName')
        name = user_info.get('displayName')
        provider_user_id = user_info.get('id')

    else:
        raise HTTPException(status_code=400, detail="Unsupported provider")

    if not email:
        raise HTTPException(
            status_code=400,
            detail="Could not retrieve email from OAuth provider"
        )

    # A-36: Link OAuth account to user (implement database logic)
    # For now, mock implementation - in production:
    # 1. Check if user exists with this email
    # 2. If yes, link OAuth account to existing user (with confirmation)
    # 3. If no, create new user account
    # 4. Store OAuth tokens encrypted (A-38)

    # A-37: Store OAuth provider linkage
    # TODO: Implement database storage for oauth_accounts table:
    # - user_id, provider, provider_user_id, access_token (encrypted), refresh_token (encrypted)

    # For now, return mock JWT (in production, call Auth Service)
    # Mock response
    response = RedirectResponse(url=redirect_uri)
    response.set_cookie(
        key="session",
        value=f"oauth_{provider}_{provider_user_id}",
        httponly=True,
        secure=True,
        samesite="lax"
    )

    return response


@router.post("/unlink/{provider}")
async def oauth_unlink(
    provider: str,
    request: Request
):
    """
    Unlink OAuth provider from user account.

    Requirement A-37: Allow unlinking providers while maintaining
    at least one authentication method.
    """
    # TODO: Implement unlinking logic
    # 1. Verify user has at least one other auth method
    # 2. Delete oauth_accounts record for this provider
    # 3. Revoke OAuth tokens if needed

    return {"success": True, "message": f"{provider} unlinked successfully"}


@router.get("/providers")
async def get_linked_providers(request: Request):
    """
    Get list of OAuth providers linked to current user.

    Requirement A-37: Show linked providers.
    """
    # TODO: Implement provider listing
    # Query oauth_accounts table for current user

    return {
        "providers": [
            {"name": "google", "linked": False},
            {"name": "github", "linked": False},
            {"name": "microsoft", "linked": False},
        ]
    }
