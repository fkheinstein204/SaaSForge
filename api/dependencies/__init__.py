"""
@file        __init__.py
@brief       Dependencies package for FastAPI dependency injection
@copyright   (c) 2025 FtsCoDe GmbH. All rights reserved.
@author      Heinstein F.
@date        2025-11-15
"""

from .auth import get_current_user, UserContext

__all__ = ["get_current_user", "UserContext"]
