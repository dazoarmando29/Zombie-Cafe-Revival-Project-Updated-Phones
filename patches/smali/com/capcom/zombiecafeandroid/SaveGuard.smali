.class public Lcom/capcom/zombiecafeandroid/SaveGuard;
.super Ljava/lang/Object;

# Save-safety helper (ZCR engine upgrade):
#   backupMaybe()   - copies a save file to <name>.bak before the engine
#                     overwrites it (throttled so multi-chunk saves do not
#                     snapshot a half-written state)
#   readBackup()    - returns the .bak contents when the primary file is
#                     unreadable/missing (used by fromNative_fileRead)
#   exportSave()    - copies a save to uninstall-surviving storage
#                     (/sdcard/ZombieCafeRevival/saves + MediaStore
#                     Download/ZombieCafeRevival on API 29+)
#   readFallback()  - .bak first, then the external export
#   restoreInternal() - restores the external export into internal storage
#                     (used by fromNative_doesFileExist so the engine sees
#                     the save again after uninstall/reinstall)
# The engine writes saves through fromNative_fileWrite as offset-based
# partial writes; those semantics are preserved exactly.

.field private static lastBackupTime:J

.field private static lastExportTime:J

.field private static lastBackupTimes:Ljava/util/HashMap;

.field private static lastExportTimes:Ljava/util/HashMap;

.method static constructor <clinit>()V
    .locals 2

    new-instance v0, Ljava/util/HashMap;

    invoke-direct {v0}, Ljava/util/HashMap;-><init>()V

    sput-object v0, Lcom/capcom/zombiecafeandroid/SaveGuard;->lastBackupTimes:Ljava/util/HashMap;

    new-instance v0, Ljava/util/HashMap;

    invoke-direct {v0}, Ljava/util/HashMap;-><init>()V

    sput-object v0, Lcom/capcom/zombiecafeandroid/SaveGuard;->lastExportTimes:Ljava/util/HashMap;

    return-void
.end method

.method public static backupMaybe(Landroid/content/Context;Ljava/lang/String;)V
    .locals 10

    if-eqz p0, :ret

    if-eqz p1, :ret

    :try_start_0
    invoke-static {}, Ljava/lang/System;->currentTimeMillis()J

    move-result-wide v0

    sget-object v2, Lcom/capcom/zombiecafeandroid/SaveGuard;->lastBackupTimes:Ljava/util/HashMap;

    invoke-virtual {v2, p1}, Ljava/util/HashMap;->get(Ljava/lang/Object;)Ljava/lang/Object;

    move-result-object v2

    if-eqz v2, :backup_now

    check-cast v2, Ljava/lang/Long;

    invoke-virtual {v2}, Ljava/lang/Long;->longValue()J

    move-result-wide v2

    sub-long v2, v0, v2

    const-wide/16 v4, 0x2710

    cmp-long v2, v2, v4

    if-ltz v2, :backup_now

    goto :ret

    :backup_now

    invoke-virtual {p0, p1}, Landroid/content/Context;->getFileStreamPath(Ljava/lang/String;)Ljava/io/File;

    move-result-object v2

    invoke-virtual {v2}, Ljava/io/File;->exists()Z

    move-result v3

    if-eqz v3, :ret

    new-instance v3, Ljava/lang/StringBuilder;

    invoke-direct {v3}, Ljava/lang/StringBuilder;-><init>()V

    invoke-virtual {v3, p1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v3

    const-string v4, ".bak"

    invoke-virtual {v3, v4}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v3

    invoke-virtual {v3}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v3

    invoke-virtual {p0, v3}, Landroid/content/Context;->getFileStreamPath(Ljava/lang/String;)Ljava/io/File;

    move-result-object v3

    new-instance v4, Ljava/io/FileInputStream;

    invoke-direct {v4, v2}, Ljava/io/FileInputStream;-><init>(Ljava/io/File;)V

    new-instance v5, Ljava/io/FileOutputStream;

    invoke-direct {v5, v3}, Ljava/io/FileOutputStream;-><init>(Ljava/io/File;)V

    const/16 v6, 0x4000

    new-array v6, v6, [B

    :copy_loop
    invoke-virtual {v4, v6}, Ljava/io/FileInputStream;->read([B)I

    move-result v7

    if-lez v7, :copy_done

    const/4 v8, 0x0

    invoke-virtual {v5, v6, v8, v7}, Ljava/io/FileOutputStream;->write([BII)V

    goto :copy_loop

    :copy_done
    invoke-virtual {v5}, Ljava/io/FileOutputStream;->close()V

    invoke-virtual {v4}, Ljava/io/FileInputStream;->close()V

    sget-object v2, Lcom/capcom/zombiecafeandroid/SaveGuard;->lastBackupTimes:Ljava/util/HashMap;

    invoke-static {v0, v1}, Ljava/lang/Long;->valueOf(J)Ljava/lang/Long;

    move-result-object v3

    invoke-virtual {v2, p1, v3}, Ljava/util/HashMap;->put(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;
    :try_end_0
    .catch Ljava/lang/Exception; {:try_start_0 .. :try_end_0} :catch_0

    :ret
    return-void

    :catch_0
    move-exception v2

    const-string v3, "zcr-save"

    const-string v4, "save backup failed"

    invoke-static {v3, v4, v2}, Landroid/util/Log;->w(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)I

    goto :ret
.end method

.method public static readBackup(Landroid/content/Context;Ljava/lang/String;)[B
    .locals 6

    if-eqz p0, :no_bak

    if-eqz p1, :no_bak

    :try_start_0
    new-instance v0, Ljava/lang/StringBuilder;

    invoke-direct {v0}, Ljava/lang/StringBuilder;-><init>()V

    invoke-virtual {v0, p1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v0

    const-string v1, ".bak"

    invoke-virtual {v0, v1}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    move-result-object v0

    invoke-virtual {v0}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v0

    invoke-virtual {p0, v0}, Landroid/content/Context;->getFileStreamPath(Ljava/lang/String;)Ljava/io/File;

    move-result-object v0

    invoke-virtual {v0}, Ljava/io/File;->exists()Z

    move-result v1

    if-nez v1, :has_bak

    goto :no_bak

    :has_bak
    new-instance v1, Ljava/io/FileInputStream;

    invoke-direct {v1, v0}, Ljava/io/FileInputStream;-><init>(Ljava/io/File;)V

    invoke-virtual {v1}, Ljava/io/FileInputStream;->available()I

    move-result v2

    new-array v2, v2, [B

    invoke-virtual {v1, v2}, Ljava/io/FileInputStream;->read([B)I

    invoke-virtual {v1}, Ljava/io/FileInputStream;->close()V

    const-string v0, "zcr-save"

    const-string v1, "serving save from backup"

    invoke-static {v0, v1}, Landroid/util/Log;->w(Ljava/lang/String;Ljava/lang/String;)I
    :try_end_0
    .catch Ljava/lang/Exception; {:try_start_0 .. :try_end_0} :catch_0

    return-object v2

    :catch_0
    move-exception v0

    invoke-virtual {v0}, Ljava/lang/Exception;->printStackTrace()V

    :no_bak
    const/4 v0, 0x0

    return-object v0
.end method

# ---------------------------------------------------------------------------
# Uninstall-surviving save export / restore
#
# Saves are copied to app-specific external storage
# (getExternalMediaDirs()[0]/ZombieCafeRevival/saves/), which survives app
# uninstall on Android 5+ (Android/media/<pkg> is not removed with the app).
# The legacy /sdcard path covers pre-scoped-storage devices and MediaStore
# Downloads (Download/ZombieCafeRevival/) keeps a user-visible copy on
# Android 10+ (its query visibility is limited for legacy-target apps, so
# it is a transparency copy, not the restore source).

.method private static exportCopy(Landroid/content/Context;Ljava/lang/String;Ljava/io/File;)V
    .locals 8

    # 1) app-specific media dir (survives uninstall)
    :try_start_0
    invoke-virtual {p0}, Landroid/content/Context;->getExternalMediaDirs()[Ljava/io/File;

    move-result-object v0

    if-eqz v0, :legacy_try

    array-length v1, v0

    if-lez v1, :legacy_try

    const/4 v1, 0x0

    aget-object v0, v0, v1

    if-eqz v0, :legacy_try

    new-instance v1, Ljava/io/File;

    const-string v2, "ZombieCafeRevival/saves"

    invoke-direct {v1, v0, v2}, Ljava/io/File;-><init>(Ljava/io/File;Ljava/lang/String;)V

    invoke-virtual {v1}, Ljava/io/File;->mkdirs()Z

    new-instance v0, Ljava/io/File;

    invoke-direct {v0, v1, p1}, Ljava/io/File;-><init>(Ljava/io/File;Ljava/lang/String;)V

    invoke-static {p2, v0}, Lcom/capcom/zombiecafeandroid/SaveGuard;->copyFile(Ljava/io/File;Ljava/io/File;)V

    const-string v0, "zcr-save"

    const-string v1, "save exported to media dir"

    invoke-static {v0, v1}, Landroid/util/Log;->i(Ljava/lang/String;Ljava/lang/String;)I
    :try_end_0
    .catch Ljava/lang/Exception; {:try_start_0 .. :try_end_0} :catch_0

    :legacy_try
    # 2) legacy /sdcard path (pre-Android-10 devices)
    :try_start_1
    invoke-static {}, Landroid/os/Environment;->getExternalStorageDirectory()Ljava/io/File;

    move-result-object v0

    new-instance v1, Ljava/io/File;

    const-string v2, "Download/ZombieCafeRevival"

    invoke-direct {v1, v0, v2}, Ljava/io/File;-><init>(Ljava/io/File;Ljava/lang/String;)V

    invoke-virtual {v1}, Ljava/io/File;->mkdirs()Z

    new-instance v0, Ljava/io/File;

    invoke-direct {v0, v1, p1}, Ljava/io/File;-><init>(Ljava/io/File;Ljava/lang/String;)V

    invoke-static {p2, v0}, Lcom/capcom/zombiecafeandroid/SaveGuard;->copyFile(Ljava/io/File;Ljava/io/File;)V

    const-string v0, "zcr-save"

    const-string v1, "save exported to legacy path"

    invoke-static {v0, v1}, Landroid/util/Log;->i(Ljava/lang/String;Ljava/lang/String;)I
    :try_end_1
    .catch Ljava/lang/Exception; {:try_start_1 .. :try_end_1} :catch_1

    :done
    return-void

    :catch_0
    move-exception v0

    const-string v1, "zcr-save"

    const-string v2, "save export (media dir) failed"

    invoke-static {v1, v2, v0}, Landroid/util/Log;->w(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)I

    goto :legacy_try

    :catch_1
    move-exception v0

    const-string v1, "zcr-save"

    const-string v2, "save export (legacy path) failed"

    invoke-static {v1, v2, v0}, Landroid/util/Log;->w(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)I

    goto :done
.end method

.method private static copyFile(Ljava/io/File;Ljava/io/File;)V
    .locals 6

    new-instance v0, Ljava/io/FileInputStream;

    invoke-direct {v0, p0}, Ljava/io/FileInputStream;-><init>(Ljava/io/File;)V

    new-instance v1, Ljava/io/FileOutputStream;

    invoke-direct {v1, p1}, Ljava/io/FileOutputStream;-><init>(Ljava/io/File;)V

    const/16 v2, 0x4000

    new-array v2, v2, [B

    :loop
    invoke-virtual {v0, v2}, Ljava/io/FileInputStream;->read([B)I

    move-result v3

    if-lez v3, :done

    const/4 v4, 0x0

    invoke-virtual {v1, v2, v4, v3}, Ljava/io/FileOutputStream;->write([BII)V

    goto :loop

    :done
    invoke-virtual {v1}, Ljava/io/FileOutputStream;->close()V

    invoke-virtual {v0}, Ljava/io/FileInputStream;->close()V

    return-void
.end method

.method private static readFile(Ljava/io/File;)[B
    .locals 6

    new-instance v0, Ljava/io/FileInputStream;

    invoke-direct {v0, p0}, Ljava/io/FileInputStream;-><init>(Ljava/io/File;)V

    new-instance v1, Ljava/io/ByteArrayOutputStream;

    invoke-direct {v1}, Ljava/io/ByteArrayOutputStream;-><init>()V

    const/16 v2, 0x4000

    new-array v2, v2, [B

    :loop
    invoke-virtual {v0, v2}, Ljava/io/FileInputStream;->read([B)I

    move-result v3

    if-lez v3, :done

    const/4 v4, 0x0

    invoke-virtual {v1, v2, v4, v3}, Ljava/io/ByteArrayOutputStream;->write([BII)V

    goto :loop

    :done
    invoke-virtual {v0}, Ljava/io/FileInputStream;->close()V

    invoke-virtual {v1}, Ljava/io/ByteArrayOutputStream;->toByteArray()[B

    move-result-object v0

    return-object v0
.end method

.method public static exportSave(Landroid/content/Context;Ljava/lang/String;)V
    .locals 10

    if-eqz p0, :ret

    if-eqz p1, :ret

    :try_start_0
    invoke-static {}, Ljava/lang/System;->currentTimeMillis()J

    move-result-wide v0

    sget-object v2, Lcom/capcom/zombiecafeandroid/SaveGuard;->lastExportTimes:Ljava/util/HashMap;

    invoke-virtual {v2, p1}, Ljava/util/HashMap;->get(Ljava/lang/Object;)Ljava/lang/Object;

    move-result-object v2

    if-eqz v2, :export_now

    check-cast v2, Ljava/lang/Long;

    invoke-virtual {v2}, Ljava/lang/Long;->longValue()J

    move-result-wide v2

    sub-long v2, v0, v2

    const-wide/16 v4, 0x2710

    cmp-long v2, v2, v4

    if-ltz v2, :export_now

    goto :ret

    :export_now
    invoke-virtual {p0, p1}, Landroid/content/Context;->getFileStreamPath(Ljava/lang/String;)Ljava/io/File;

    move-result-object v2

    invoke-virtual {v2}, Ljava/io/File;->exists()Z

    move-result v3

    if-eqz v3, :ret

    invoke-static {p0, p1, v2}, Lcom/capcom/zombiecafeandroid/SaveGuard;->exportCopy(Landroid/content/Context;Ljava/lang/String;Ljava/io/File;)V

    # user-visible transparency copy in Downloads (Android 10+)
    :try_start_1
    sget v3, Landroid/os/Build$VERSION;->SDK_INT:I

    const/16 v4, 0x1d

    if-lt v3, v4, :export_done

    invoke-static {p0, p1, v2}, Lcom/capcom/zombiecafeandroid/SaveExport29;->export(Landroid/content/Context;Ljava/lang/String;Ljava/io/File;)V
    :try_end_1
    .catch Ljava/lang/Exception; {:try_start_1 .. :try_end_1} :catch_1

    :export_done
    sget-object v2, Lcom/capcom/zombiecafeandroid/SaveGuard;->lastExportTimes:Ljava/util/HashMap;

    invoke-static {v0, v1}, Ljava/lang/Long;->valueOf(J)Ljava/lang/Long;

    move-result-object v3

    invoke-virtual {v2, p1, v3}, Ljava/util/HashMap;->put(Ljava/lang/Object;Ljava/lang/Object;)Ljava/lang/Object;

    :ret
    return-void

    :catch_0
    move-exception v2

    const-string v3, "zcr-save"

    const-string v4, "save export failed"

    invoke-static {v3, v4, v2}, Landroid/util/Log;->w(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)I

    goto :ret

    :catch_1
    move-exception v2

    const-string v3, "zcr-save"

    const-string v4, "save export (MediaStore) failed"

    invoke-static {v3, v4, v2}, Landroid/util/Log;->w(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)I

    goto :export_done
.end method

.method public static readExternal(Landroid/content/Context;Ljava/lang/String;)[B
    .locals 5

    if-eqz p0, :fail

    if-eqz p1, :fail

    # 1) app-specific media dir
    :try_start_0
    invoke-virtual {p0}, Landroid/content/Context;->getExternalMediaDirs()[Ljava/io/File;

    move-result-object v0

    if-eqz v0, :download_try

    array-length v1, v0

    if-lez v1, :download_try

    const/4 v1, 0x0

    aget-object v0, v0, v1

    if-eqz v0, :download_try

    new-instance v1, Ljava/io/File;

    const-string v2, "ZombieCafeRevival/saves"

    invoke-direct {v1, v0, v2}, Ljava/io/File;-><init>(Ljava/io/File;Ljava/lang/String;)V

    new-instance v0, Ljava/io/File;

    invoke-direct {v0, v1, p1}, Ljava/io/File;-><init>(Ljava/io/File;Ljava/lang/String;)V

    invoke-virtual {v0}, Ljava/io/File;->exists()Z

    move-result v1

    if-eqz v1, :download_try

    invoke-static {v0}, Lcom/capcom/zombiecafeandroid/SaveGuard;->readFile(Ljava/io/File;)[B

    move-result-object v0

    const-string v1, "zcr-save"

    const-string v2, "serving save from media dir export"

    invoke-static {v1, v2}, Landroid/util/Log;->w(Ljava/lang/String;Ljava/lang/String;)I
    :try_end_0
    .catch Ljava/lang/Exception; {:try_start_0 .. :try_end_0} :catch_0

    return-object v0

    :catch_0
    move-exception v0

    goto :download_try

    :download_try
    # 2) physical Download folder (MediaStore export location)
    :try_start_1
    invoke-static {}, Landroid/os/Environment;->getExternalStorageDirectory()Ljava/io/File;

    move-result-object v0

    new-instance v1, Ljava/io/File;

    const-string v2, "Download/ZombieCafeRevival"

    invoke-direct {v1, v0, v2}, Ljava/io/File;-><init>(Ljava/io/File;Ljava/lang/String;)V

    new-instance v0, Ljava/io/File;

    invoke-direct {v0, v1, p1}, Ljava/io/File;-><init>(Ljava/io/File;Ljava/lang/String;)V

    invoke-virtual {v0}, Ljava/io/File;->exists()Z

    move-result v1

    if-eqz v1, :media_store

    invoke-static {v0}, Lcom/capcom/zombiecafeandroid/SaveGuard;->readFile(Ljava/io/File;)[B

    move-result-object v0

    const-string v1, "zcr-save"

    const-string v2, "serving save from Download folder"

    invoke-static {v1, v2}, Landroid/util/Log;->w(Ljava/lang/String;Ljava/lang/String;)I
    :try_end_1
    .catch Ljava/lang/Exception; {:try_start_1 .. :try_end_1} :catch_1

    return-object v0

    :catch_1
    move-exception v0

    goto :media_store

    :media_store
    # 3) MediaStore query (may be invisible to legacy-target apps; last resort)
    :try_start_2
    sget v0, Landroid/os/Build$VERSION;->SDK_INT:I

    const/16 v1, 0x1d

    if-lt v0, v1, :fail

    invoke-static {p0, p1}, Lcom/capcom/zombiecafeandroid/SaveExport29;->restoreBytes(Landroid/content/Context;Ljava/lang/String;)[B

    move-result-object v0
    :try_end_2
    .catch Ljava/lang/Exception; {:try_start_2 .. :try_end_2} :catch_2

    return-object v0

    :catch_2
    move-exception v0

    const-string v1, "zcr-save"

    const-string v2, "save restore (external) failed"

    invoke-static {v1, v2, v0}, Landroid/util/Log;->w(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)I

    :fail
    const/4 v0, 0x0

    return-object v0
.end method

.method public static readFallback(Landroid/content/Context;Ljava/lang/String;)[B
    .locals 1

    if-eqz p0, :fail

    if-eqz p1, :fail

    invoke-static {p0, p1}, Lcom/capcom/zombiecafeandroid/SaveGuard;->readBackup(Landroid/content/Context;Ljava/lang/String;)[B

    move-result-object v0

    if-eqz v0, :try_external

    return-object v0

    :try_external
    invoke-static {p0, p1}, Lcom/capcom/zombiecafeandroid/SaveGuard;->readExternal(Landroid/content/Context;Ljava/lang/String;)[B

    move-result-object v0

    return-object v0

    :fail
    const/4 v0, 0x0

    return-object v0
.end method

.method public static restoreInternal(Landroid/content/Context;Ljava/lang/String;)Z
    .locals 4

    const/4 v0, 0x0

    if-eqz p0, :ret

    if-eqz p1, :ret

    :try_start_0
    invoke-virtual {p0, p1}, Landroid/content/Context;->getFileStreamPath(Ljava/lang/String;)Ljava/io/File;

    move-result-object v1

    invoke-virtual {v1}, Ljava/io/File;->exists()Z

    move-result v1

    if-eqz v1, :need_restore

    const/4 v0, 0x1

    return v0

    :need_restore
    invoke-static {p0, p1}, Lcom/capcom/zombiecafeandroid/SaveGuard;->readExternal(Landroid/content/Context;Ljava/lang/String;)[B

    move-result-object v1

    if-nez v1, :have_bytes

    invoke-static {p0, p1}, Lcom/capcom/zombiecafeandroid/SaveGuard;->readBackup(Landroid/content/Context;Ljava/lang/String;)[B

    move-result-object v1

    if-eqz v1, :ret

    :have_bytes
    const/4 v2, 0x0

    invoke-virtual {p0, p1, v2}, Landroid/content/Context;->openFileOutput(Ljava/lang/String;I)Ljava/io/FileOutputStream;

    move-result-object v2

    invoke-virtual {v2, v1}, Ljava/io/FileOutputStream;->write([B)V

    invoke-virtual {v2}, Ljava/io/FileOutputStream;->close()V

    const-string v1, "zcr-save"

    const-string v2, "restored save into internal storage"

    invoke-static {v1, v2}, Landroid/util/Log;->w(Ljava/lang/String;Ljava/lang/String;)I
    :try_end_0
    .catch Ljava/lang/Exception; {:try_start_0 .. :try_end_0} :catch_0

    const/4 v0, 0x1

    return v0

    :catch_0
    move-exception v1

    const-string v2, "zcr-save"

    const-string v3, "save restore into internal failed"

    invoke-static {v2, v3, v1}, Landroid/util/Log;->w(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)I

    :ret
    return v0
.end method
