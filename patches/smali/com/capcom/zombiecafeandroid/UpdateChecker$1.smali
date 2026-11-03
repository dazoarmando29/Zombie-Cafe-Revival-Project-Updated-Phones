.class Lcom/capcom/zombiecafeandroid/UpdateChecker$1;
.super Ljava/lang/Thread;

# ZCR: worker thread for UpdateChecker. mode 0 = check for updates,
# 1 = download the pending APK, 2 = delayed delete after install.

.field mode:I

.method public constructor <init>(I)V
    .locals 0

    invoke-direct {p0}, Ljava/lang/Thread;-><init>()V

    iput p1, p0, Lcom/capcom/zombiecafeandroid/UpdateChecker$1;->mode:I

    return-void
.end method

.method public run()V
    .locals 5

    .catch Ljava/lang/InterruptedException; {:try_start .. :try_end} :catch_0

    iget v0, p0, Lcom/capcom/zombiecafeandroid/UpdateChecker$1;->mode:I

    if-nez v0, :not_check

    invoke-static {}, Lcom/capcom/zombiecafeandroid/UpdateChecker;->check()V

    return-void

    :not_check
    const/4 v1, 0x1

    if-ne v0, v1, :cleanup

    invoke-static {}, Lcom/capcom/zombiecafeandroid/UpdateChecker;->doDownload()V

    return-void

    :cleanup
    const-wide/32 v0, 0x2bf20

    :try_start
    invoke-static {v0, v1}, Ljava/lang/Thread;->sleep(J)V
    :try_end
    goto :slept

    :catch_0
    move-exception v0

    :slept
    sget-object v0, Lcom/capcom/zombiecafeandroid/UpdateChecker;->sActivity:Ljava/lang/ref/WeakReference;

    if-eqz v0, :done

    invoke-virtual {v0}, Ljava/lang/ref/WeakReference;->get()Ljava/lang/Object;

    move-result-object v0

    check-cast v0, Landroid/app/Activity;

    if-eqz v0, :done

    invoke-static {v0}, Lcom/capcom/zombiecafeandroid/UpdateChecker;->cleanupOld(Landroid/content/Context;)V

    :done
    return-void
.end method
