package com.example.qrfastpay

import android.content.Context
import android.content.Intent
import android.net.Uri

data class PaymentDetails(
    val rawUri: String,
    val payeeName: String,
    val payeeVpa: String,
    val amount: String?,
    val currency: String,
    val transactionNote: String?
)

object PaymentHandler {

    fun parse(payload: String): PaymentDetails? {
        if (!payload.startsWith("upi://pay", ignoreCase = true)) {
            return null
        }
        return try {
            val uri = Uri.parse(payload)
            val vpa = uri.getQueryParameter("pa") ?: return null
            val name = uri.getQueryParameter("pn") ?: vpa
            val amount = uri.getQueryParameter("am")
            val currency = uri.getQueryParameter("cu") ?: "INR"
            val note = uri.getQueryParameter("tn")

            PaymentDetails(
                rawUri = payload,
                payeeName = name,
                payeeVpa = vpa,
                amount = amount,
                currency = currency,
                transactionNote = note
            )
        } catch (_: Exception) {
            null
        }
    }

    fun launchUpiIntent(context: Context, payment: PaymentDetails) {
        val intent = Intent(Intent.ACTION_VIEW).apply {
            data = Uri.parse(payment.rawUri)
        }
        val chooser = Intent.createChooser(intent, "Pay ${payment.payeeName} via...")
        chooser.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK)
        context.startActivity(chooser)
    }
}
