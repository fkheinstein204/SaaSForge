"""gRPC client wrapper for Payment Service."""

import grpc
import os
from typing import List, Optional
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'generated'))

from payment_pb2 import (
    CreateSubscriptionRequest, UpdateSubscriptionRequest,
    CancelSubscriptionRequest, GetSubscriptionRequest,
    SubscriptionResponse,
    AddPaymentMethodRequest, PaymentMethodResponse,
    RemovePaymentMethodRequest, RemovePaymentMethodResponse,
    GetInvoiceRequest, InvoiceResponse,
    RecordUsageRequest, RecordUsageResponse
)
from payment_pb2_grpc import PaymentServiceStub


class PaymentClient:
    """Client for Payment Service gRPC calls."""

    def __init__(self, host: str = None, port: int = None):
        self.host = host or os.getenv("PAYMENT_SERVICE_HOST", "localhost")
        self.port = port or int(os.getenv("PAYMENT_SERVICE_PORT", "50053"))
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

        self.stub = PaymentServiceStub(self.channel)

    def create_subscription(
        self,
        tenant_id: str,
        plan_id: str,
        payment_method_id: str,
        quantity: int,
        trial_days: Optional[int],
        idempotency_key: str,
        metadata: List[tuple]
    ) -> SubscriptionResponse:
        """Create new subscription."""
        request = CreateSubscriptionRequest(
            tenant_id=tenant_id,
            plan_id=plan_id,
            payment_method_id=payment_method_id,
            quantity=quantity,
            idempotency_key=idempotency_key
        )
        if trial_days is not None:
            request.trial_days = trial_days

        return self.stub.CreateSubscription(request, metadata=metadata)

    def update_subscription(
        self,
        tenant_id: str,
        subscription_id: str,
        plan_id: Optional[str],
        quantity: Optional[int],
        metadata: List[tuple]
    ) -> SubscriptionResponse:
        """Update existing subscription."""
        request = UpdateSubscriptionRequest(
            tenant_id=tenant_id,
            subscription_id=subscription_id
        )
        if plan_id:
            request.plan_id = plan_id
        if quantity is not None:
            request.quantity = quantity

        return self.stub.UpdateSubscription(request, metadata=metadata)

    def cancel_subscription(
        self,
        tenant_id: str,
        subscription_id: str,
        immediate: bool,
        metadata: List[tuple]
    ) -> SubscriptionResponse:
        """Cancel subscription."""
        request = CancelSubscriptionRequest(
            tenant_id=tenant_id,
            subscription_id=subscription_id,
            immediate=immediate
        )
        return self.stub.CancelSubscription(request, metadata=metadata)

    def get_subscription(
        self,
        tenant_id: str,
        subscription_id: str,
        metadata: List[tuple]
    ) -> SubscriptionResponse:
        """Get subscription details."""
        request = GetSubscriptionRequest(
            tenant_id=tenant_id,
            subscription_id=subscription_id
        )
        return self.stub.GetSubscription(request, metadata=metadata)

    def add_payment_method(
        self,
        tenant_id: str,
        stripe_payment_method_id: str,
        metadata: List[tuple]
    ) -> PaymentMethodResponse:
        """Add payment method."""
        request = AddPaymentMethodRequest(
            tenant_id=tenant_id,
            stripe_payment_method_id=stripe_payment_method_id
        )
        return self.stub.AddPaymentMethod(request, metadata=metadata)

    def remove_payment_method(
        self,
        tenant_id: str,
        payment_method_id: str,
        metadata: List[tuple]
    ) -> RemovePaymentMethodResponse:
        """Remove payment method."""
        request = RemovePaymentMethodRequest(
            tenant_id=tenant_id,
            payment_method_id=payment_method_id
        )
        return self.stub.RemovePaymentMethod(request, metadata=metadata)

    def get_invoice(
        self,
        tenant_id: str,
        invoice_id: str,
        metadata: List[tuple]
    ) -> InvoiceResponse:
        """Get invoice details."""
        request = GetInvoiceRequest(
            tenant_id=tenant_id,
            invoice_id=invoice_id
        )
        return self.stub.GetInvoice(request, metadata=metadata)

    def record_usage(
        self,
        tenant_id: str,
        subscription_id: str,
        metric_name: str,
        quantity: int,
        timestamp: int,
        metadata: List[tuple]
    ) -> RecordUsageResponse:
        """Record usage for metered billing."""
        request = RecordUsageRequest(
            tenant_id=tenant_id,
            subscription_id=subscription_id,
            metric_name=metric_name,
            quantity=quantity,
            timestamp=timestamp
        )
        return self.stub.RecordUsage(request, metadata=metadata)

    def close(self):
        """Close gRPC channel."""
        if self.channel:
            self.channel.close()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()
