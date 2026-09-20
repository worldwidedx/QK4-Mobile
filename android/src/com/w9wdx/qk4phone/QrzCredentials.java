package com.w9wdx.qk4phone;

import android.content.Context;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.util.AtomicFile;
import java.io.File;
import java.io.FileOutputStream;
import java.nio.ByteBuffer;
import java.nio.charset.StandardCharsets;
import java.security.KeyStore;
import java.util.Arrays;
import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;

/** AES-GCM ciphertext is private and excluded from backup; the AES key never
 * leaves Android Keystore. Fail closed if the key or ciphertext is unavailable. */
public final class QrzCredentials {
    private static final String ALIAS = "QK4.QRZ.Logbook.v1";
    private static AtomicFile file(Context c) {
        return new AtomicFile(new File(c.getNoBackupFilesDir(), "qrz-key.v1"));
    }
    private static SecretKey key(boolean create) throws Exception {
        KeyStore store = KeyStore.getInstance("AndroidKeyStore");
        store.load(null);
        if (store.containsAlias(ALIAS)) return (SecretKey) store.getKey(ALIAS, null);
        if (!create) throw new IllegalStateException("Credential key unavailable");
        KeyGenerator gen = KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES, "AndroidKeyStore");
        gen.init(new KeyGenParameterSpec.Builder(ALIAS,
            KeyProperties.PURPOSE_ENCRYPT | KeyProperties.PURPOSE_DECRYPT)
            .setBlockModes(KeyProperties.BLOCK_MODE_GCM)
            .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
            .setKeySize(256).setRandomizedEncryptionRequired(true).build());
        return gen.generateKey();
    }
    // Prefix distinguishes an empty store from a decryption error across JNI.
    public static synchronized String read(Context c) {
        if (!file(c).getBaseFile().exists()) return "OK:";
        byte[] plain = null;
        try {
            byte[] data = file(c).readFully();
            if (data.length < 29 || data.length > 8192 || data[0] != 1) return "ERROR";
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            cipher.init(Cipher.DECRYPT_MODE, key(false), new GCMParameterSpec(128, data, 1, 12));
            cipher.updateAAD(ALIAS.getBytes(StandardCharsets.UTF_8));
            plain = cipher.doFinal(data, 13, data.length - 13);
            return "OK:" + new String(plain, StandardCharsets.UTF_8);
        } catch (Exception ignored) { return "ERROR"; }
        finally { if (plain != null) Arrays.fill(plain, (byte) 0); }
    }
    public static synchronized boolean write(Context c, String value) {
        if (value == null || value.isEmpty() || value.length() > 4096) return false;
        AtomicFile target = file(c);
        FileOutputStream stream = null;
        byte[] plain = value.getBytes(StandardCharsets.UTF_8);
        try {
            Cipher cipher = Cipher.getInstance("AES/GCM/NoPadding");
            cipher.init(Cipher.ENCRYPT_MODE, key(true));
            cipher.updateAAD(ALIAS.getBytes(StandardCharsets.UTF_8));
            byte[] encrypted = cipher.doFinal(plain);
            byte[] iv = cipher.getIV();
            if (iv.length != 12) return false;
            byte[] data = ByteBuffer.allocate(1 + iv.length + encrypted.length)
                .put((byte) 1).put(iv).put(encrypted).array();
            stream = target.startWrite();
            stream.write(data);
            target.finishWrite(stream);
            return true;
        } catch (Exception ignored) {
            if (stream != null) target.failWrite(stream);
            return false;
        } finally { Arrays.fill(plain, (byte) 0); }
    }
    public static synchronized boolean clear(Context c) {
        try {
            file(c).delete();
            KeyStore store = KeyStore.getInstance("AndroidKeyStore");
            store.load(null);
            store.deleteEntry(ALIAS);
            return !file(c).getBaseFile().exists();
        } catch (Exception ignored) { return false; }
    }
    private QrzCredentials() {}
}
