package org.nopeforge.nopegl

import android.content.ContentResolver
import android.content.Context
import android.content.ContextWrapper
import android.content.pm.ProviderInfo
import android.test.mock.MockContentResolver
import androidx.test.ext.junit.runners.AndroidJUnit4
import androidx.test.platform.app.InstrumentationRegistry
import org.junit.Assert.*
import org.junit.Before
import org.junit.Test
import org.junit.runner.RunWith
import org.nopeforge.nopegl.Config
import java.io.File
import java.io.FileOutputStream
import java.nio.ByteBuffer
import org.nopeforge.nopegl.Config as NGLConfig
import org.nopeforge.nopegl.Context as NGLContext


class MockContext(private val contentResolver: ContentResolver, base: Context?) :
    ContextWrapper(base) {
    override fun getContentResolver(): ContentResolver {
        return contentResolver
    }
}

@RunWith(AndroidJUnit4::class)
class NopeGLTest {
    private fun createContext(backend: Int) : NGLContext {
        val config = NGLConfig()
        config.backend = backend
        config.offscreen = true
        config.width = 256
        config.height = 256
        config.clearColor = floatArrayOf(1.0F, 1.0F, 0.0F, 1.0F)

        val ctx = NGLContext()
        val ret = ctx.configure(config)
        assertEquals(ret, 0)

        return ctx
    }

    private fun offscreenCtx(backend: Int) {
        val appContext = InstrumentationRegistry.getInstrumentation().targetContext
        NGLContext.nativeInit(appContext)

        val ctx = createContext(backend)

        var ret = ctx.draw(0.0)
        assertEquals(ret, 0)

        ret = ctx.resize(256, 256)
        assert(ret != 0)

        val captureBuffer = ByteBuffer.allocateDirect(256 * 256 * 4)
        ret = ctx.setCaptureBuffer(captureBuffer)
        assertEquals(ret, 0)

        ret = ctx.draw(0.0)
        assertEquals(ret, 0)

        val buffer = captureBuffer.asIntBuffer()
        assertEquals((0xFFFF00FF).toUInt(), buffer[0].toUInt())

        ctx.finalize()
    }

    @Test
    fun offscreenOpenGLCtx() {
        offscreenCtx(Config.BACKEND_OPENGLES)
    }

    @Test
    fun offscreenVulkanCtx() {
        offscreenCtx(Config.BACKEND_VULKAN)
    }

    private fun checkAsset(applicationContext: Context, name: String) : Boolean {
        return File(getAssetPath(applicationContext, name)).exists()
    }

    private fun copyAsset(applicationContext: Context, name: String) {
        val inputStream = applicationContext.resources.assets.open(name)
        inputStream.copyTo(FileOutputStream(getAssetPath(applicationContext, name)))
    }

    private fun getAssetPath(applicationContext: Context, name : String) : String {
        return applicationContext.cacheDir!!.path + "/" + name
    }

    @Before
    fun initAssets() {
        val context = InstrumentationRegistry.getInstrumentation().targetContext
        if (!checkAsset(context,"cat.mp4"))
            copyAsset(context, "cat.mp4")
    }

    @Test
    fun loadMediaScene() {
        val appContext = InstrumentationRegistry.getInstrumentation().targetContext

        val providerInfo = ProviderInfo()
        providerInfo.authority = NGLContentProvider.AUTHORITY

        val contentProvider = NGLContentProvider()
        contentProvider.attachInfo(appContext, providerInfo)

        val contentResolver = MockContentResolver(appContext)
        contentResolver.addProvider(NGLContentProvider.AUTHORITY, contentProvider)

        val fakeCtx = MockContext(contentResolver, appContext)
        NGLContext.nativeInit(fakeCtx)

        val ctx = createContext(Config.BACKEND_OPENGLES)
        ctx.loadScene("""
            # Nope.GL v0.11.0
            # duration=403Z9000000000000
            # aspect_ratio=320/240
            # framerate=60/1
            Mdia filename:content://%s/medias%s
            Tex2 data_src:1
            Quad
            Rtex texture:2 geometry:1
        """.trimIndent().format(
            NGLContentProvider.AUTHORITY,
            getAssetPath(appContext,"cat.mp4"))
        )

        var i = 0
        val nbFrames = 10
        while (i < nbFrames) {
            ctx.draw(i * 1.0 / 60.0)
            i++
        }

        ctx.finalize()
    }
}