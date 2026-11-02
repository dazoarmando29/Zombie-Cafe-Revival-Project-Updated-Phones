.class public Lcom/capcom/zombiecafeandroid/AlarmReceiver;
.super Landroid/content/BroadcastReceiver;


# static fields
.field private static final a:I


# direct methods
.method static constructor <clinit>()V
    .locals 1

    sget v0, Lcom/capcom/zombiecafeandroid/ZombieCafeAndroid;->notificationNumber:I

    sput v0, Lcom/capcom/zombiecafeandroid/AlarmReceiver;->a:I

    return-void
.end method

.method public constructor <init>()V
    .locals 0

    invoke-direct {p0}, Landroid/content/BroadcastReceiver;-><init>()V

    return-void
.end method


# virtual methods
# ZCR: the original body used Notification.setLatestEventInfo + a channel-less
# notify(), which silently does nothing on Android 8+, and a mutability-less
# PendingIntent, which throws on Android 12+. Delegates to NotifHelper which
# posts through NotificationChannel / Notification.Builder with proper flags.
.method public onReceive(Landroid/content/Context;Landroid/content/Intent;)V
    .locals 4

    if-eqz p2, :done

    :try_start_0
    invoke-virtual {p2}, Landroid/content/Intent;->getExtras()Landroid/os/Bundle;

    move-result-object v0

    if-eqz v0, :done

    const-string v1, "alarm_message"

    invoke-virtual {v0, v1}, Landroid/os/Bundle;->getString(Ljava/lang/String;)Ljava/lang/String;

    move-result-object v1

    if-eqz v1, :done

    const-string v2, "type"

    invoke-virtual {v0, v2}, Landroid/os/Bundle;->getInt(Ljava/lang/String;)I

    move-result v0

    sget v2, Lcom/capcom/zombiecafeandroid/AlarmReceiver;->a:I

    add-int/2addr v2, v0

    invoke-static {p1, v2, v1}, Lcom/capcom/zombiecafeandroid/NotifHelper;->post(Landroid/content/Context;ILjava/lang/String;)V

    const-string v0, "AlarmReceiver"

    const-string v2, "posted notification"

    invoke-static {v0, v2}, Landroid/util/Log;->i(Ljava/lang/String;Ljava/lang/String;)I
    :try_end_0
    .catch Ljava/lang/Exception; {:try_start_0 .. :try_end_0} :catch_0

    :done
    return-void

    :catch_0
    move-exception v0

    invoke-virtual {v0}, Ljava/lang/Exception;->printStackTrace()V

    return-void
.end method
