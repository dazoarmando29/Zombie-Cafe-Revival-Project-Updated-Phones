.class public final Lcom/capcom/zombiecafeandroid/DebugMenuItemClickListener;
.super Ljava/lang/Object;
.implements Landroid/view/View$OnClickListener;


# instance fields
.field private final a:I


# direct methods
.method public constructor <init>(I)V
    .locals 0

    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    iput p1, p0, Lcom/capcom/zombiecafeandroid/DebugMenuItemClickListener;->a:I

    return-void
.end method

.method private static toast(Landroid/content/Context;Ljava/lang/String;)V
    .locals 2

    const/4 v0, 0x0

    invoke-static {p0, p1, v0}, Landroid/widget/Toast;->makeText(Landroid/content/Context;Ljava/lang/CharSequence;I)Landroid/widget/Toast;

    move-result-object v0

    invoke-virtual {v0}, Landroid/widget/Toast;->show()V

    return-void
.end method


# virtual methods
.method public onClick(Landroid/view/View;)V
    .locals 3

    invoke-virtual {p1}, Landroid/view/View;->getContext()Landroid/content/Context;

    move-result-object v0

    iget v1, p0, Lcom/capcom/zombiecafeandroid/DebugMenuItemClickListener;->a:I

    packed-switch v1, :pswitch_data_0

    :goto_0
    return-void

    :pswitch_0
    const/4 v1, 0x1

    invoke-static {v1}, Lcom/capcom/zombiecafeandroid/ZombieCafeAndroid;->PurchaseAndroidToxin(I)V

    const-string v1, "Toxin +1"

    invoke-static {v0, v1}, Lcom/capcom/zombiecafeandroid/DebugMenuItemClickListener;->toast(Landroid/content/Context;Ljava/lang/String;)V

    goto :goto_0

    :pswitch_1
    const/16 v1, 0x3e7

    invoke-static {v1}, Lcom/capcom/zombiecafeandroid/ZombieCafeAndroid;->DebugSetToxin(I)V

    const-string v1, "Toxin set to 999"

    invoke-static {v0, v1}, Lcom/capcom/zombiecafeandroid/DebugMenuItemClickListener;->toast(Landroid/content/Context;Ljava/lang/String;)V

    goto :goto_0

    :pswitch_2
    const/4 v1, 0x1

    invoke-static {v1}, Lcom/capcom/zombiecafeandroid/ZombieCafeAndroid;->DebugAddMoney(I)V

    const-string v1, "Money +1"

    invoke-static {v0, v1}, Lcom/capcom/zombiecafeandroid/DebugMenuItemClickListener;->toast(Landroid/content/Context;Ljava/lang/String;)V

    goto :goto_0

    :pswitch_3
    const/16 v1, 0x2710

    invoke-static {v1}, Lcom/capcom/zombiecafeandroid/ZombieCafeAndroid;->DebugAddMoney(I)V

    const-string v1, "Money +10,000"

    invoke-static {v0, v1}, Lcom/capcom/zombiecafeandroid/DebugMenuItemClickListener;->toast(Landroid/content/Context;Ljava/lang/String;)V

    goto :goto_0

    :pswitch_4
    const v1, 0xf423f

    invoke-static {v1}, Lcom/capcom/zombiecafeandroid/ZombieCafeAndroid;->DebugAddMoney(I)V

    const-string v1, "Money set to 999,999"

    invoke-static {v0, v1}, Lcom/capcom/zombiecafeandroid/DebugMenuItemClickListener;->toast(Landroid/content/Context;Ljava/lang/String;)V

    goto :goto_0

    :pswitch_5
    const/16 v1, 0x2710

    invoke-static {v1}, Lcom/capcom/zombiecafeandroid/ZombieCafeAndroid;->DebugAddXp(I)V

    const-string v1, "XP +10,000"

    invoke-static {v0, v1}, Lcom/capcom/zombiecafeandroid/DebugMenuItemClickListener;->toast(Landroid/content/Context;Ljava/lang/String;)V

    goto :goto_0

    :pswitch_6
    const v1, 0xf423f

    invoke-static {v1}, Lcom/capcom/zombiecafeandroid/ZombieCafeAndroid;->DebugSetXp(I)V

    const-string v1, "XP set to 999,999"

    invoke-static {v0, v1}, Lcom/capcom/zombiecafeandroid/DebugMenuItemClickListener;->toast(Landroid/content/Context;Ljava/lang/String;)V

    goto :goto_0

    :pswitch_data_0
    .packed-switch 0x0
        :pswitch_0
        :pswitch_1
        :pswitch_2
        :pswitch_3
        :pswitch_4
        :pswitch_5
        :pswitch_6
    .end packed-switch
.end method
