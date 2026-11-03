.class Lcom/capcom/zombiecafeandroid/UpdateChecker$3;
.super Landroid/os/Handler;

# ZCR: UI-thread dispatcher for UpdateChecker.
#   1 = show the update dialog
#   3 = dismiss progress + open the system installer
#   4 = dismiss progress + failure toast

.method public constructor <init>(Landroid/os/Looper;)V
    .locals 0

    invoke-direct {p0, p1}, Landroid/os/Handler;-><init>(Landroid/os/Looper;)V

    return-void
.end method

.method public handleMessage(Landroid/os/Message;)V
    .locals 5

    iget v0, p1, Landroid/os/Message;->what:I

    packed-switch v0, :pswitch_data

    return-void

    :pswitch_0
    sget-object v0, Lcom/capcom/zombiecafeandroid/UpdateChecker;->sActivity:Ljava/lang/ref/WeakReference;

    if-eqz v0, :d0

    invoke-virtual {v0}, Ljava/lang/ref/WeakReference;->get()Ljava/lang/Object;

    move-result-object v0

    check-cast v0, Landroid/app/Activity;

    if-eqz v0, :d0

    new-instance v1, Landroid/app/AlertDialog$Builder;

    invoke-direct {v1, v0}, Landroid/app/AlertDialog$Builder;-><init>(Landroid/content/Context;)V

    const-string v2, "Update available"

    invoke-virtual {v1, v2}, Landroid/app/AlertDialog$Builder;->setTitle(Ljava/lang/CharSequence;)Landroid/app/AlertDialog$Builder;

    new-instance v2, Ljava/lang/StringBuilder;

    invoke-direct {v2}, Ljava/lang/StringBuilder;-><init>()V

    const-string v3, "A new version of Zombie Cafe Revival is ready:\n"

    invoke-virtual {v2, v3}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    sget-object v3, Lcom/capcom/zombiecafeandroid/UpdateChecker;->sPendingTag:Ljava/lang/String;

    invoke-virtual {v2, v3}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    const-string v3, "\n\nDownload and install it now?"

    invoke-virtual {v2, v3}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    invoke-virtual {v2}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v2

    invoke-virtual {v1, v2}, Landroid/app/AlertDialog$Builder;->setMessage(Ljava/lang/CharSequence;)Landroid/app/AlertDialog$Builder;

    const-string v2, "Install"

    new-instance v3, Lcom/capcom/zombiecafeandroid/UpdateChecker$2;

    invoke-direct {v3}, Lcom/capcom/zombiecafeandroid/UpdateChecker$2;-><init>()V

    invoke-virtual {v1, v2, v3}, Landroid/app/AlertDialog$Builder;->setPositiveButton(Ljava/lang/CharSequence;Landroid/content/DialogInterface$OnClickListener;)Landroid/app/AlertDialog$Builder;

    const-string v2, "Later"

    const/4 v3, 0x0

    invoke-virtual {v1, v2, v3}, Landroid/app/AlertDialog$Builder;->setNegativeButton(Ljava/lang/CharSequence;Landroid/content/DialogInterface$OnClickListener;)Landroid/app/AlertDialog$Builder;

    invoke-virtual {v1}, Landroid/app/AlertDialog$Builder;->show()Landroid/app/AlertDialog;

    :d0
    return-void

    :pswitch_1
    return-void

    :pswitch_2
    sget-object v0, Lcom/capcom/zombiecafeandroid/UpdateChecker;->sProgress:Landroid/app/ProgressDialog;

    if-eqz v0, :nopd

    invoke-virtual {v0}, Landroid/app/ProgressDialog;->dismiss()V

    const/4 v0, 0x0

    sput-object v0, Lcom/capcom/zombiecafeandroid/UpdateChecker;->sProgress:Landroid/app/ProgressDialog;

    :nopd
    sget-object v0, Lcom/capcom/zombiecafeandroid/UpdateChecker;->sActivity:Ljava/lang/ref/WeakReference;

    if-eqz v0, :d1

    invoke-virtual {v0}, Ljava/lang/ref/WeakReference;->get()Ljava/lang/Object;

    move-result-object v0

    check-cast v0, Landroid/app/Activity;

    if-eqz v0, :d1

    invoke-static {v0}, Lcom/capcom/zombiecafeandroid/UpdateChecker;->fireInstall(Landroid/content/Context;)V

    :d1
    return-void

    :pswitch_3
    sget-object v0, Lcom/capcom/zombiecafeandroid/UpdateChecker;->sProgress:Landroid/app/ProgressDialog;

    if-eqz v0, :nopd2

    invoke-virtual {v0}, Landroid/app/ProgressDialog;->dismiss()V

    const/4 v0, 0x0

    sput-object v0, Lcom/capcom/zombiecafeandroid/UpdateChecker;->sProgress:Landroid/app/ProgressDialog;

    :nopd2
    sget-object v0, Lcom/capcom/zombiecafeandroid/UpdateChecker;->sActivity:Ljava/lang/ref/WeakReference;

    if-eqz v0, :d2

    invoke-virtual {v0}, Ljava/lang/ref/WeakReference;->get()Ljava/lang/Object;

    move-result-object v0

    check-cast v0, Landroid/app/Activity;

    if-eqz v0, :d2

    const-string v1, "Update download failed. Try again later."

    const/4 v2, 0x0

    invoke-static {v0, v1, v2}, Landroid/widget/Toast;->makeText(Landroid/content/Context;Ljava/lang/CharSequence;I)Landroid/widget/Toast;

    move-result-object v0

    invoke-virtual {v0}, Landroid/widget/Toast;->show()V

    :d2
    return-void

    :pswitch_data
    .packed-switch 0x1
        :pswitch_0
        :pswitch_1
        :pswitch_2
        :pswitch_3
    .end packed-switch
.end method
