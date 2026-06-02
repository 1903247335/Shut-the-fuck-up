/**
 * Shut-the-fuck-up Android 控制端 — 主界面
 */
package com.shutthefuckup.app

import android.os.Bundle
import android.view.View
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity
import androidx.core.content.ContextCompat
import androidx.lifecycle.lifecycleScope
import com.shutthefuckup.app.databinding.ActivityMainBinding
import kotlinx.coroutines.Dispatchers
import kotlinx.coroutines.launch
import kotlinx.coroutines.withContext

class MainActivity : AppCompatActivity() {

    private lateinit var binding: ActivityMainBinding

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        binding = ActivityMainBinding.inflate(layoutInflater)
        setContentView(binding.root)

        binding.btnStart.setOnClickListener { setRunning(true) }
        binding.btnStop.setOnClickListener { setRunning(false) }
        binding.btnApplyInterval.setOnClickListener { applyInterval() }
        binding.btnRefresh.setOnClickListener { refreshStatus() }

        refreshStatus()
    }

    private fun serverUrl(): String {
        val raw = binding.etServer.text?.toString()?.trim().orEmpty()
        return if (raw.isEmpty()) getString(R.string.server_default) else raw
    }

    private fun refreshStatus() {
        lifecycleScope.launch {
            setLoading(true)
            try {
                val status = withContext(Dispatchers.IO) {
                    ApiClient(serverUrl()).getStatus()
                }
                updateUi(status.state, status.interval)
            } catch (_: Exception) {
                Toast.makeText(this@MainActivity, R.string.toast_error, Toast.LENGTH_SHORT).show()
            } finally {
                setLoading(false)
            }
        }
    }

    private fun setRunning(start: Boolean) {
        lifecycleScope.launch {
            setLoading(true)
            try {
                val state = withContext(Dispatchers.IO) {
                    ApiClient(serverUrl()).setState(start)
                }
                val interval = binding.etInterval.text?.toString()?.toIntOrNull() ?: 30
                updateUi(state, interval)
                Toast.makeText(
                    this@MainActivity,
                    if (start) R.string.toast_start_ok else R.string.toast_stop_ok,
                    Toast.LENGTH_SHORT
                ).show()
            } catch (_: Exception) {
                Toast.makeText(this@MainActivity, R.string.toast_error, Toast.LENGTH_SHORT).show()
            } finally {
                setLoading(false)
            }
        }
    }

    private fun applyInterval() {
        val seconds = binding.etInterval.text?.toString()?.toIntOrNull()
        if (seconds == null || seconds < 1) {
            Toast.makeText(this, "请输入大于 0 的秒数", Toast.LENGTH_SHORT).show()
            return
        }

        lifecycleScope.launch {
            setLoading(true)
            try {
                val interval = withContext(Dispatchers.IO) {
                    ApiClient(serverUrl()).setInterval(seconds)
                }
                binding.etInterval.setText(interval.toString())
                Toast.makeText(this@MainActivity, R.string.toast_interval_ok, Toast.LENGTH_SHORT).show()
            } catch (_: Exception) {
                Toast.makeText(this@MainActivity, R.string.toast_error, Toast.LENGTH_SHORT).show()
            } finally {
                setLoading(false)
            }
        }
    }

    private fun updateUi(state: String, interval: Int) {
        val running = state == "start"
        binding.tvStatus.text = if (running) {
            getString(R.string.status_start)
        } else {
            getString(R.string.status_stop)
        }
        binding.tvStatus.setTextColor(
            ContextCompat.getColor(
                this,
                if (running) R.color.status_start else R.color.status_stop
            )
        )
        binding.tvInterval.text = "联网间隔：${interval} 秒"
        binding.etInterval.setText(interval.toString())
    }

    private fun setLoading(loading: Boolean) {
        binding.progressBar.visibility = if (loading) View.VISIBLE else View.GONE
        binding.btnStart.isEnabled = !loading
        binding.btnStop.isEnabled = !loading
        binding.btnApplyInterval.isEnabled = !loading
        binding.btnRefresh.isEnabled = !loading
    }
}
