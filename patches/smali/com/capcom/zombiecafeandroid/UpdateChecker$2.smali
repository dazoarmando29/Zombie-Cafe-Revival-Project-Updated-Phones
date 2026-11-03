.class Lcom/capcom/zombiecafeandroid/UpdateChecker$2;
.super Ljava/lang/Object;
.implements Landroid/content/DialogInterface$OnClickListener;

# ZCR: update dialog "Install" button.

.method public constructor <init>()V
    .locals 0

    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method

.method public onClick(Landroid/content/DialogInterface;I)V
    .locals 0

    invoke-static {}, Lcom/capcom/zombiecafeandroid/UpdateChecker;->beginDownload()V

    return-void
.end method
