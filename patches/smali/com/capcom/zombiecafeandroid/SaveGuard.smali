.class public Lcom/capcom/zombiecafeandroid/SaveGuard;
.super Ljava/lang/Object;

# Save-safety helper (ZCR engine upgrade):
#   backupMaybe() - copies a save file to <name>.bak before the engine
#                   overwrites it (throttled so multi-chunk saves do not
#                   snapshot a half-written state)
#   readBackup()  - returns the .bak contents when the primary file is
#                   unreadable/missing (used by fromNative_fileRead)
# The engine writes saves through fromNative_fileWrite as offset-based
# partial writes; those semantics are preserved exactly.

.field private static lastBackupTime:J

.method public static backupMaybe(Landroid/content/Context;Ljava/lang/String;)V
    .locals 10

    if-eqz p0, :ret

    if-eqz p1, :ret

    :try_start_0
    invoke-static {}, Ljava/lang/System;->currentTimeMillis()J

    move-result-wide v0

    sget-wide v2, Lcom/capcom/zombiecafeandroid/SaveGuard;->lastBackupTime:J

    sub-long v2, v0, v2

    const-wide/16 v4, 0x2710

    cmp-long v2, v2, v4

    if-gez v2, :ret

    invoke-virtual {p0, p1}, Landroid/content/Context;->getFileStreamPath(Ljava/lang/String;)Ljava/io/File;

    move-result-object v2

    invoke-virtual {v2}, Ljava/io/File;->exists()Z

    move-result v3

    if-nez v3, :ret

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

    if-gtz v7, :copy_done

    const/4 v8, 0x0

    invoke-virtual {v5, v6, v8, v7}, Ljava/io/FileOutputStream;->write([BII)V

    goto :copy_loop

    :copy_done
    invoke-virtual {v5}, Ljava/io/FileOutputStream;->close()V

    invoke-virtual {v4}, Ljava/io/FileInputStream;->close()V

    sput-wide v0, Lcom/capcom/zombiecafeandroid/SaveGuard;->lastBackupTime:J
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
