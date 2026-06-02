/**
 * Shut-the-fuck-up Android 控制端 — HTTP API 封装
 */
package com.shutthefuckup.app

import okhttp3.MediaType.Companion.toMediaType
import okhttp3.OkHttpClient
import okhttp3.Request
import okhttp3.RequestBody.Companion.toRequestBody
import org.json.JSONObject
import java.util.concurrent.TimeUnit

class ApiClient(baseUrl: String) {

    private val base = baseUrl.trimEnd('/')

    private val client = OkHttpClient.Builder()
        .connectTimeout(10, TimeUnit.SECONDS)
        .readTimeout(10, TimeUnit.SECONDS)
        .writeTimeout(10, TimeUnit.SECONDS)
        .build()

    private val jsonMedia = "application/json; charset=utf-8".toMediaType()
    private val emptyBody = "".toRequestBody(jsonMedia)

    data class Status(val state: String, val interval: Int)

    @Throws(Exception::class)
    fun getStatus(): Status {
        val stateJson = JSONObject(get("/get_state"))
        val intervalJson = JSONObject(get("/get_interval"))
        return Status(
            state = stateJson.optString("state", "stop"),
            interval = intervalJson.optInt("interval", 5)
        )
    }

    @Throws(Exception::class)
    fun setState(start: Boolean): String {
        val stateVal = if (start) "start" else "stop"
        val json = JSONObject(post("/set_state?state=$stateVal", emptyBody))
        if (json.optString("msg") == "error") {
            throw Exception("set_state failed")
        }
        return json.optString("state", stateVal)
    }

    @Throws(Exception::class)
    fun setInterval(seconds: Int): Int {
        val queryJson = try {
            JSONObject(post("/set_interval?interval=$seconds", emptyBody))
        } catch (_: Exception) {
            null
        }
        if (queryJson != null && queryJson.optString("msg") != "error") {
            return queryJson.optInt("interval", seconds)
        }

        val body = JSONObject().put("interval", seconds).toString()
        val json = JSONObject(post("/set_interval", body.toRequestBody(jsonMedia)))
        if (json.optString("msg") == "error" && !json.has("interval")) {
            throw Exception("set_interval not supported")
        }
        return json.optInt("interval", seconds)
    }

    @Throws(Exception::class)
    private fun get(path: String): String {
        val request = Request.Builder().url("$base$path").get().build()
        client.newCall(request).execute().use { response ->
            if (!response.isSuccessful) throw Exception("HTTP ${response.code}")
            return response.body?.string() ?: throw Exception("empty body")
        }
    }

    @Throws(Exception::class)
    private fun post(path: String, body: okhttp3.RequestBody): String {
        val request = Request.Builder()
            .url("$base$path")
            .post(body)
            .build()
        client.newCall(request).execute().use { response ->
            if (!response.isSuccessful) throw Exception("HTTP ${response.code}")
            return response.body?.string() ?: throw Exception("empty body")
        }
    }
}
