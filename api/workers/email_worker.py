"""
@file        email_worker.py
@brief       Background worker for processing email queue
@copyright   (c) 2025 FtsCoDe GmbH. All rights reserved.
@author      Heinstein F.
@date        2025-11-15
"""

import asyncio
import json
import logging
from datetime import datetime
from typing import Dict, Any
from redis import Redis
import os

from services.email_service import EmailService, EmailPriority

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


class EmailQueueWorker:
    """
    Background worker that processes emails from Redis queue

    Features:
    - Processes queues in priority order (critical > high > normal > low)
    - Retry failed emails with exponential backoff
    - Dead letter queue for permanently failed emails
    - Graceful shutdown handling
    """

    def __init__(self):
        self.email_service = EmailService()

        redis_host = os.getenv("REDIS_HOST", "localhost")
        redis_port = int(os.getenv("REDIS_PORT", "6379"))
        self.redis = Redis(host=redis_host, port=redis_port, decode_responses=True)

        self.queue_keys = {
            EmailPriority.CRITICAL: "email:queue:critical",
            EmailPriority.HIGH: "email:queue:high",
            EmailPriority.NORMAL: "email:queue:normal",
            EmailPriority.LOW: "email:queue:low",
        }

        self.dead_letter_queue = "email:queue:dead_letter"
        self.max_retries = 3
        self.running = False

    async def start(self):
        """Start the worker"""
        self.running = True
        logger.info("Email queue worker started")

        try:
            while self.running:
                # Process queues in priority order
                for priority in [EmailPriority.CRITICAL, EmailPriority.HIGH, EmailPriority.NORMAL, EmailPriority.LOW]:
                    await self._process_queue(priority)

                # Sleep between queue checks
                await asyncio.sleep(1)

        except KeyboardInterrupt:
            logger.info("Shutdown signal received")
        finally:
            await self.shutdown()

    async def shutdown(self):
        """Graceful shutdown"""
        self.running = False
        logger.info("Email queue worker stopped")

    async def _process_queue(self, priority: EmailPriority):
        """Process one email from the specified priority queue"""
        queue_key = self.queue_keys[priority]

        try:
            # Pop email from queue (RPOP for FIFO)
            email_json = self.redis.rpop(queue_key)

            if not email_json:
                return  # Queue is empty

            email_data = json.loads(email_json)
            logger.info(f"Processing {priority} email to {email_data['to_email']}")

            # Send the email
            success = await self.email_service._send_now(
                to_email=email_data["to_email"],
                subject=email_data["subject"],
                html_content=email_data["html_content"],
                plain_content=email_data.get("plain_content"),
                template_id=email_data.get("template_id"),
                template_data=email_data.get("template_data"),
            )

            if not success:
                # Email failed - handle retry
                await self._handle_failed_email(email_data, priority)

        except json.JSONDecodeError as e:
            logger.error(f"Invalid JSON in queue: {e}")
        except Exception as e:
            logger.error(f"Error processing email from {priority} queue: {e}")

    async def _handle_failed_email(self, email_data: Dict[str, Any], priority: EmailPriority):
        """Handle failed email with retry logic"""
        retry_count = email_data.get("retry_count", 0)

        if retry_count >= self.max_retries:
            # Max retries exceeded - move to dead letter queue
            logger.error(f"Email to {email_data['to_email']} failed after {retry_count} retries - moving to DLQ")
            email_data["failed_at"] = datetime.utcnow().isoformat()
            email_data["failure_reason"] = "Max retries exceeded"
            self.redis.lpush(self.dead_letter_queue, json.dumps(email_data))
            return

        # Increment retry count and re-queue with exponential backoff
        retry_count += 1
        email_data["retry_count"] = retry_count
        backoff_seconds = 2 ** retry_count  # 2, 4, 8 seconds

        logger.info(f"Retrying email to {email_data['to_email']} in {backoff_seconds}s (attempt {retry_count}/{self.max_retries})")

        # Wait for backoff period
        await asyncio.sleep(backoff_seconds)

        # Re-add to queue
        queue_key = self.queue_keys[priority]
        self.redis.lpush(queue_key, json.dumps(email_data))

    async def get_queue_stats(self) -> Dict[str, int]:
        """Get current queue statistics"""
        stats = {}
        for priority, queue_key in self.queue_keys.items():
            stats[priority.value] = self.redis.llen(queue_key)
        stats["dead_letter"] = self.redis.llen(self.dead_letter_queue)
        return stats


async def main():
    """Main entry point for the worker"""
    worker = EmailQueueWorker()
    await worker.start()


if __name__ == "__main__":
    asyncio.run(main())
