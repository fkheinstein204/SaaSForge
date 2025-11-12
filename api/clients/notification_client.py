"""gRPC client wrapper for Notification Service."""

import grpc
import os
from typing import List, Dict, Optional
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'generated'))

from notification_pb2 import (
    SendEmailRequest, SendSMSRequest, SendPushRequest,
    TriggerWebhookRequest, GetNotificationStatusRequest,
    UpdatePreferencesRequest, RegisterWebhookRequest,
    NotificationResponse, PreferencesResponse, WebhookResponse
)
from notification_pb2_grpc import NotificationServiceStub


class NotificationClient:
    """Client for Notification Service gRPC calls."""

    def __init__(self, host: str = None, port: int = None):
        self.host = host or os.getenv("NOTIFICATION_SERVICE_HOST", "localhost")
        self.port = port or int(os.getenv("NOTIFICATION_SERVICE_PORT", "50054"))
        self.address = f"{self.host}:{self.port}"

        # Load mTLS certificates for secure service-to-service communication
        ca_cert_path = os.getenv("GRPC_CA_CERT_PATH", "certs/ca.crt")
        client_cert_path = os.getenv("GRPC_CLIENT_CERT_PATH", "certs/client.crt")
        client_key_path = os.getenv("GRPC_CLIENT_KEY_PATH", "certs/client.key")

        try:
            with open(ca_cert_path, "rb") as f:
                ca_cert = f.read()
            with open(client_cert_path, "rb") as f:
                client_cert = f.read()
            with open(client_key_path, "rb") as f:
                client_key = f.read()

            credentials = grpc.ssl_channel_credentials(
                root_certificates=ca_cert,
                private_key=client_key,
                certificate_chain=client_cert
            )

            self.channel = grpc.secure_channel(self.address, credentials)
        except FileNotFoundError as e:
            print(f"WARNING: mTLS certs not found ({e}), using insecure channel")
            self.channel = grpc.insecure_channel(self.address)

        self.stub = NotificationServiceStub(self.channel)

    def send_email(
        self,
        tenant_id: str,
        user_id: str,
        to: str,
        subject: str,
        body_html: str,
        body_text: Optional[str],
        template_id: Optional[str],
        template_vars: Dict[str, str],
        metadata: List[tuple]
    ) -> NotificationResponse:
        """Send email notification."""
        request = SendEmailRequest(
            tenant_id=tenant_id,
            user_id=user_id,
            to=to,
            subject=subject,
            body_html=body_html,
            template_vars=template_vars or {}
        )
        if body_text:
            request.body_text = body_text
        if template_id:
            request.template_id = template_id

        return self.stub.SendEmail(request, metadata=metadata)

    def send_sms(
        self,
        tenant_id: str,
        user_id: str,
        to: str,
        message: str,
        metadata: List[tuple]
    ) -> NotificationResponse:
        """Send SMS notification."""
        request = SendSMSRequest(
            tenant_id=tenant_id,
            user_id=user_id,
            to=to,
            message=message
        )
        return self.stub.SendSMS(request, metadata=metadata)

    def send_push(
        self,
        tenant_id: str,
        user_id: str,
        title: str,
        body: str,
        data: Dict[str, str],
        metadata: List[tuple]
    ) -> NotificationResponse:
        """Send push notification."""
        request = SendPushRequest(
            tenant_id=tenant_id,
            user_id=user_id,
            title=title,
            body=body,
            data=data or {}
        )
        return self.stub.SendPush(request, metadata=metadata)

    def trigger_webhook(
        self,
        tenant_id: str,
        webhook_id: str,
        event_type: str,
        payload: str,
        metadata: List[tuple]
    ) -> NotificationResponse:
        """Trigger webhook."""
        request = TriggerWebhookRequest(
            tenant_id=tenant_id,
            webhook_id=webhook_id,
            event_type=event_type,
            payload=payload
        )
        return self.stub.TriggerWebhook(request, metadata=metadata)

    def get_notification_status(
        self,
        tenant_id: str,
        notification_id: str,
        metadata: List[tuple]
    ) -> NotificationResponse:
        """Get notification status."""
        request = GetNotificationStatusRequest(
            tenant_id=tenant_id,
            notification_id=notification_id
        )
        return self.stub.GetNotificationStatus(request, metadata=metadata)

    def update_preferences(
        self,
        tenant_id: str,
        user_id: str,
        email_enabled: bool,
        sms_enabled: bool,
        push_enabled: bool,
        marketing_emails: bool,
        metadata: List[tuple]
    ) -> PreferencesResponse:
        """Update notification preferences."""
        request = UpdatePreferencesRequest(
            tenant_id=tenant_id,
            user_id=user_id,
            email_enabled=email_enabled,
            sms_enabled=sms_enabled,
            push_enabled=push_enabled,
            marketing_emails=marketing_emails
        )
        return self.stub.UpdatePreferences(request, metadata=metadata)

    def register_webhook(
        self,
        tenant_id: str,
        url: str,
        events: List[str],
        secret: Optional[str],
        metadata: List[tuple]
    ) -> WebhookResponse:
        """Register webhook."""
        request = RegisterWebhookRequest(
            tenant_id=tenant_id,
            url=url,
            events=events
        )
        if secret:
            request.secret = secret

        return self.stub.RegisterWebhook(request, metadata=metadata)

    def close(self):
        """Close gRPC channel."""
        if self.channel:
            self.channel.close()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
