.class public Lcom/capcom/zombiecafeandroid/NotifHelper;
.super Ljava/lang/Object;

# Modern-Android notification plumbing for the 2011 engine (ZCR).
#
# The original code scheduled alarms with a mutability-less PendingIntent
# (throws on Android 12+) and posted with setLatestEventInfo + no channel
# (dropped silently on Android 8+). This helper keeps the same icons, sound
# and behaviour while working on Android 5..15:
#
#   ensureChannel() - creates the NotificationChannel on API 26+
#   schedule()      - AlarmManager with immutable PendingIntent, exact when
#                     permitted, inexact fallback
#   post()          - builds + posts the notification (channel on 26+)

.field private static channelReady:Z

.method public static ensureChannel(Landroid/content/Context;)V
    .locals 5

    sget-boolean v0, Lcom/capcom/zombiecafeandroid/NotifHelper;->channelReady:Z

    if-nez v0, :done

    sget v0, Landroid/os/Build$VERSION;->SDK_INT:I

    const/16 v1, 0x1a

    if-lt v0, v1, :mark

    :try_start_0
    const-string v0, "notification"

    invoke-virtual {p0, v0}, Landroid/content/Context;->getSystemService(Ljava/lang/String;)Ljava/lang/Object;

    move-result-object v0

    check-cast v0, Landroid/app/NotificationManager;

    if-eqz v0, :mark

    new-instance v1, Landroid/app/NotificationChannel;

    const-string v2, "zcr_game"

    const-string v3, "Zombie Cafe"

    const/4 v4, 0x4

    invoke-direct {v1, v2, v3, v4}, Landroid/app/NotificationChannel;-><init>(Ljava/lang/String;Ljava/lang/CharSequence;I)V

    const-string v2, "android.resource://com.capcom.zombiecafeandroid/2131034115"

    invoke-static {v2}, Landroid/net/Uri;->parse(Ljava/lang/String;)Landroid/net/Uri;

    move-result-object v2

    new-instance v3, Landroid/media/AudioAttributes$Builder;

    invoke-direct {v3}, Landroid/media/AudioAttributes$Builder;-><init>()V

    const/4 v4, 0x5

    invoke-virtual {v3, v4}, Landroid/media/AudioAttributes$Builder;->setUsage(I)Landroid/media/AudioAttributes$Builder;

    move-result-object v3

    const/4 v4, 0x4

    invoke-virtual {v3, v4}, Landroid/media/AudioAttributes$Builder;->setContentType(I)Landroid/media/AudioAttributes$Builder;

    move-result-object v3

    invoke-virtual {v3}, Landroid/media/AudioAttributes$Builder;->build()Landroid/media/AudioAttributes;

    move-result-object v3

    invoke-virtual {v1, v2, v3}, Landroid/app/NotificationChannel;->setSound(Landroid/net/Uri;Landroid/media/AudioAttributes;)V

    invoke-virtual {v0, v1}, Landroid/app/NotificationManager;->createNotificationChannel(Landroid/app/NotificationChannel;)V
    :try_end_0
    .catch Ljava/lang/Exception; {:try_start_0 .. :try_end_0} :catch_0

    goto :mark

    :catch_0
    move-exception v0

    :mark
    const/4 v0, 0x1

    sput-boolean v0, Lcom/capcom/zombiecafeandroid/NotifHelper;->channelReady:Z

    :done
    return-void
.end method

.method public static schedule(Landroid/content/Context;ILandroid/content/Intent;J)V
    .locals 6

    :try_start_0
    const/high16 v0, 0x8000000

    const/high16 v1, 0x4000000

    or-int/2addr v0, v1

    invoke-static {p0, p1, p2, v0}, Landroid/app/PendingIntent;->getBroadcast(Landroid/content/Context;ILandroid/content/Intent;I)Landroid/app/PendingIntent;

    move-result-object v4

    const-string v0, "alarm"

    invoke-virtual {p0, v0}, Landroid/content/Context;->getSystemService(Ljava/lang/String;)Ljava/lang/Object;

    move-result-object v0

    check-cast v0, Landroid/app/AlarmManager;

    if-eqz v0, :done

    const/4 v1, 0x0

    move-wide v2, p3

    sget p0, Landroid/os/Build$VERSION;->SDK_INT:I

    const/16 p1, 0x17

    if-lt p0, p1, :fallback

    const/16 p1, 0x1f

    if-lt p0, p1, :exact_check_done

    invoke-virtual {v0}, Landroid/app/AlarmManager;->canScheduleExactAlarms()Z

    move-result p0

    if-eqz p0, :inexact

    invoke-virtual/range {v0 .. v4}, Landroid/app/AlarmManager;->setExactAndAllowWhileIdle(IJLandroid/app/PendingIntent;)V

    goto :done

    :exact_check_done
    invoke-virtual/range {v0 .. v4}, Landroid/app/AlarmManager;->setExactAndAllowWhileIdle(IJLandroid/app/PendingIntent;)V

    goto :done

    :inexact
    invoke-virtual/range {v0 .. v4}, Landroid/app/AlarmManager;->setAndAllowWhileIdle(IJLandroid/app/PendingIntent;)V

    goto :done

    :fallback
    invoke-virtual/range {v0 .. v4}, Landroid/app/AlarmManager;->set(IJLandroid/app/PendingIntent;)V
    :try_end_0
    .catch Ljava/lang/Exception; {:try_start_0 .. :try_end_0} :catch_0

    :done
    return-void

    :catch_0
    move-exception v0

    invoke-virtual {v0}, Ljava/lang/Exception;->printStackTrace()V

    return-void
.end method

.method public static post(Landroid/content/Context;ILjava/lang/String;)V
    .locals 6

    if-eqz p2, :done

    :try_start_0
    invoke-static {p0}, Lcom/capcom/zombiecafeandroid/NotifHelper;->ensureChannel(Landroid/content/Context;)V

    new-instance v0, Landroid/content/Intent;

    const-class v1, Lcom/capcom/zombiecafeandroid/ZombieCafeAndroid;

    invoke-direct {v0, p0, v1}, Landroid/content/Intent;-><init>(Landroid/content/Context;Ljava/lang/Class;)V

    const/high16 v1, 0x10000000

    invoke-virtual {v0, v1}, Landroid/content/Intent;->addFlags(I)Landroid/content/Intent;

    const/high16 v1, 0x8000000

    const/high16 v2, 0x4000000

    or-int/2addr v1, v2

    const/4 v2, 0x0

    invoke-static {p0, v2, v0, v1}, Landroid/app/PendingIntent;->getActivity(Landroid/content/Context;ILandroid/content/Intent;I)Landroid/app/PendingIntent;

    move-result-object v0

    sget v1, Landroid/os/Build$VERSION;->SDK_INT:I

    const/16 v3, 0x1a

    if-lt v1, v3, :leg_ctor

    new-instance v1, Landroid/app/Notification$Builder;

    const-string v3, "zcr_game"

    invoke-direct {v1, p0, v3}, Landroid/app/Notification$Builder;-><init>(Landroid/content/Context;Ljava/lang/String;)V

    goto :built

    :leg_ctor
    new-instance v1, Landroid/app/Notification$Builder;

    invoke-direct {v1, p0}, Landroid/app/Notification$Builder;-><init>(Landroid/content/Context;)V

    :built
    const-string v3, "Zombie Cafe"

    invoke-virtual {v1, v3}, Landroid/app/Notification$Builder;->setContentTitle(Ljava/lang/CharSequence;)Landroid/app/Notification$Builder;

    move-result-object v1

    invoke-virtual {v1, p2}, Landroid/app/Notification$Builder;->setContentText(Ljava/lang/CharSequence;)Landroid/app/Notification$Builder;

    move-result-object v1

    const v3, 0x7f020003

    invoke-virtual {v1, v3}, Landroid/app/Notification$Builder;->setSmallIcon(I)Landroid/app/Notification$Builder;

    move-result-object v1

    invoke-virtual {v1, v0}, Landroid/app/Notification$Builder;->setContentIntent(Landroid/app/PendingIntent;)Landroid/app/Notification$Builder;

    move-result-object v0

    const/4 v1, 0x1

    invoke-virtual {v0, v1}, Landroid/app/Notification$Builder;->setAutoCancel(Z)Landroid/app/Notification$Builder;

    move-result-object v0

    invoke-virtual {v0, v1}, Landroid/app/Notification$Builder;->setDefaults(I)Landroid/app/Notification$Builder;

    move-result-object v0

    invoke-virtual {v0}, Landroid/app/Notification$Builder;->build()Landroid/app/Notification;

    move-result-object v0

    const-string v1, "notification"

    invoke-virtual {p0, v1}, Landroid/content/Context;->getSystemService(Ljava/lang/String;)Ljava/lang/Object;

    move-result-object v1

    check-cast v1, Landroid/app/NotificationManager;

    if-eqz v1, :done

    invoke-virtual {v1, p1, v0}, Landroid/app/NotificationManager;->notify(ILandroid/app/Notification;)V
    :try_end_0
    .catch Ljava/lang/Exception; {:try_start_0 .. :try_end_0} :catch_0

    :done
    return-void

    :catch_0
    move-exception v0

    invoke-virtual {v0}, Ljava/lang/Exception;->printStackTrace()V

    return-void
.end method
