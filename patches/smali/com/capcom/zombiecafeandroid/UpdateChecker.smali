.class public final Lcom/capcom/zombiecafeandroid/UpdateChecker;
.super Ljava/lang/Object;

# ZCR: in-game updater (2026-11-03). Checks the GitHub releases feed on
# launch, shows an in-game dialog when a newer build exists, downloads the
# matching variant APK, opens the system installer through ZcrFileProvider,
# and deletes the downloaded APK afterwards (delayed + on next launch).
#
# Version scheme: versionName is the ZCR version ("0.3.0"); legacy builds
# carry ".... ZCR v0.1.3" - parseVer() understands both. Release tags are
# "v0.3.0-android15" / "v0.3.0-modmenu-android15"; the asset name decides
# the variant ("...ModMenu.apk" = mod menu build).

.field static sActivity:Ljava/lang/ref/WeakReference;
.field static sHandler:Landroid/os/Handler;
.field static sNeedsInstallRetry:Z
.field static sPendingTag:Ljava/lang/String;
.field static sPendingUrl:Ljava/lang/String;
.field static sProgress:Landroid/app/ProgressDialog;
.field static sStarted:Z

.method public static init(Landroid/app/Activity;)V
    .locals 2

    const-string v0, "ZCR-Updater"

    const-string v1, "init"

    invoke-static {v0, v1}, Landroid/util/Log;->i(Ljava/lang/String;Ljava/lang/String;)I

    sget-boolean v0, Lcom/capcom/zombiecafeandroid/UpdateChecker;->sStarted:Z

    if-nez v0, :already

    const/4 v0, 0x1

    sput-boolean v0, Lcom/capcom/zombiecafeandroid/UpdateChecker;->sStarted:Z

    new-instance v0, Ljava/lang/ref/WeakReference;

    invoke-direct {v0, p0}, Ljava/lang/ref/WeakReference;-><init>(Ljava/lang/Object;)V

    sput-object v0, Lcom/capcom/zombiecafeandroid/UpdateChecker;->sActivity:Ljava/lang/ref/WeakReference;

    new-instance v0, Lcom/capcom/zombiecafeandroid/UpdateChecker$3;

    invoke-static {}, Landroid/os/Looper;->getMainLooper()Landroid/os/Looper;

    move-result-object v1

    invoke-direct {v0, v1}, Lcom/capcom/zombiecafeandroid/UpdateChecker$3;-><init>(Landroid/os/Looper;)V

    sput-object v0, Lcom/capcom/zombiecafeandroid/UpdateChecker;->sHandler:Landroid/os/Handler;

    invoke-static {p0}, Lcom/capcom/zombiecafeandroid/UpdateChecker;->cleanupOld(Landroid/content/Context;)V

    new-instance v0, Lcom/capcom/zombiecafeandroid/UpdateChecker$1;

    const/4 v1, 0x0

    invoke-direct {v0, v1}, Lcom/capcom/zombiecafeandroid/UpdateChecker$1;-><init>(I)V

    invoke-virtual {v0}, Ljava/lang/Thread;->start()V

    :already
    return-void
.end method

# Startup state machine for a pending update (update.version marker):
#  - no marker: delete any stray APK (post-update leftover)
#  - marker + already on that version: delete APK + marker (install
#    succeeded - this is the "delete the APK after updating" step)
#  - marker + older + APK present: install now, or wait for the
#    "install unknown apps" grant and continue automatically from onResume
.method static cleanupOld(Landroid/content/Context;)V
    .locals 5

    const-string v2, "ZCR-Updater"

    const-string v3, "cleanupOld"

    invoke-static {v2, v3}, Landroid/util/Log;->i(Ljava/lang/String;Ljava/lang/String;)I

    invoke-static {p0}, Lcom/capcom/zombiecafeandroid/UpdateChecker;->resolveApkFile(Landroid/content/Context;)Ljava/io/File;

    move-result-object v0

    invoke-static {p0}, Lcom/capcom/zombiecafeandroid/UpdateChecker;->resolveVerFile(Landroid/content/Context;)Ljava/io/File;

    move-result-object v1

    invoke-virtual {v1}, Ljava/io/File;->exists()Z

    move-result v2

    if-eqz v2, :stray

    invoke-static {v1}, Lcom/capcom/zombiecafeandroid/UpdateChecker;->readVer(Ljava/io/File;)Ljava/lang/String;

    move-result-object v2

    if-eqz v2, :stray

    invoke-static {p0}, Lcom/capcom/zombiecafeandroid/UpdateChecker;->currentVersionName(Landroid/content/Context;)Ljava/lang/String;

    move-result-object v3

    invoke-static {v3, v2}, Lcom/capcom/zombiecafeandroid/UpdateChecker;->compareVersions(Ljava/lang/String;Ljava/lang/String;)I

    move-result v3

    if-ltz v3, :pending

    invoke-virtual {v0}, Ljava/io/File;->delete()Z

    invoke-virtual {v1}, Ljava/io/File;->delete()Z

    return-void

    :pending
    invoke-virtual {v0}, Ljava/io/File;->exists()Z

    move-result v3

    if-eqz v3, :dropver

    invoke-static {p0}, Lcom/capcom/zombiecafeandroid/UpdateChecker;->canInstall(Landroid/content/Context;)Z

    move-result v3

    if-eqz v3, :waitperm

    invoke-static {p0}, Lcom/capcom/zombiecafeandroid/UpdateChecker;->fireInstall(Landroid/content/Context;)V

    return-void

    :waitperm
    const/4 v3, 0x1

    sput-boolean v3, Lcom/capcom/zombiecafeandroid/UpdateChecker;->sNeedsInstallRetry:Z

    return-void

    :dropver
    invoke-virtual {v1}, Ljava/io/File;->delete()Z

    return-void

    :stray
    invoke-virtual {v0}, Ljava/io/File;->exists()Z

    move-result v2

    if-eqz v2, :done

    invoke-virtual {v0}, Ljava/io/File;->delete()Z

    :done
    return-void
.end method

.method static resolveApkFile(Landroid/content/Context;)Ljava/io/File;
    .locals 3

    const/4 v0, 0x0

    invoke-virtual {p0, v0}, Landroid/content/Context;->getExternalFilesDir(Ljava/lang/String;)Ljava/io/File;

    move-result-object v1

    if-eqz v1, :internal

    :gotbase
    new-instance v0, Ljava/io/File;

    const-string v2, "zcr_update"

    invoke-direct {v0, v1, v2}, Ljava/io/File;-><init>(Ljava/io/File;Ljava/lang/String;)V

    invoke-virtual {v0}, Ljava/io/File;->mkdirs()Z

    new-instance v1, Ljava/io/File;

    const-string v2, "update.apk"

    invoke-direct {v1, v0, v2}, Ljava/io/File;-><init>(Ljava/io/File;Ljava/lang/String;)V

    return-object v1

    :internal
    invoke-virtual {p0}, Landroid/content/Context;->getFilesDir()Ljava/io/File;

    move-result-object v1

    goto :gotbase
.end method

.method static resolveVerFile(Landroid/content/Context;)Ljava/io/File;
    .locals 3

    const/4 v0, 0x0

    invoke-virtual {p0, v0}, Landroid/content/Context;->getExternalFilesDir(Ljava/lang/String;)Ljava/io/File;

    move-result-object v1

    if-eqz v1, :internal

    :gotbase
    new-instance v0, Ljava/io/File;

    const-string v2, "zcr_update"

    invoke-direct {v0, v1, v2}, Ljava/io/File;-><init>(Ljava/io/File;Ljava/lang/String;)V

    invoke-virtual {v0}, Ljava/io/File;->mkdirs()Z

    new-instance v1, Ljava/io/File;

    const-string v2, "update.version"

    invoke-direct {v1, v0, v2}, Ljava/io/File;-><init>(Ljava/io/File;Ljava/lang/String;)V

    return-object v1

    :internal
    invoke-virtual {p0}, Landroid/content/Context;->getFilesDir()Ljava/io/File;

    move-result-object v1

    goto :gotbase
.end method

.method static canInstall(Landroid/content/Context;)Z
    .locals 2

    sget v0, Landroid/os/Build$VERSION;->SDK_INT:I

    const/16 v1, 0x1a

    if-lt v0, v1, :yes

    invoke-virtual {p0}, Landroid/content/Context;->getPackageManager()Landroid/content/pm/PackageManager;

    move-result-object v0

    invoke-virtual {v0}, Landroid/content/pm/PackageManager;->canRequestPackageInstalls()Z

    move-result v0

    return v0

    :yes
    const/4 v0, 0x1

    return v0
.end method

.method static currentVersionName(Landroid/content/Context;)Ljava/lang/String;
    .locals 3

    .catch Ljava/lang/Throwable; {:try_start .. :try_end} :catch_0

    const/4 v0, 0x0

    :try_start
    invoke-virtual {p0}, Landroid/content/Context;->getPackageManager()Landroid/content/pm/PackageManager;

    move-result-object v1

    invoke-virtual {p0}, Landroid/content/Context;->getPackageName()Ljava/lang/String;

    move-result-object v2

    invoke-virtual {v1, v2, v0}, Landroid/content/pm/PackageManager;->getPackageInfo(Ljava/lang/String;I)Landroid/content/pm/PackageInfo;

    move-result-object v1

    iget-object v0, v1, Landroid/content/pm/PackageInfo;->versionName:Ljava/lang/String;
    :try_end
    goto :have

    :catch_0
    move-exception v1

    :have
    return-object v0
.end method

# Fires the system installer for the downloaded APK. If the "install
# unknown apps" permission is missing, remember to retry from onResume so
# the install continues automatically once the user grants it in Settings.
.method static fireInstall(Landroid/content/Context;)V
    .locals 4

    invoke-static {p0}, Lcom/capcom/zombiecafeandroid/UpdateChecker;->resolveApkFile(Landroid/content/Context;)Ljava/io/File;

    move-result-object v0

    invoke-virtual {v0}, Ljava/io/File;->exists()Z

    move-result v1

    if-eqz v1, :done

    invoke-static {p0}, Lcom/capcom/zombiecafeandroid/UpdateChecker;->canInstall(Landroid/content/Context;)Z

    move-result v1

    if-eqz v1, :noperm

    :gotperm
    new-instance v1, Landroid/content/Intent;

    const-string v2, "android.intent.action.VIEW"

    invoke-direct {v1, v2}, Landroid/content/Intent;-><init>(Ljava/lang/String;)V

    const-string v2, "content://com.capcom.zombiecafeandroid.zcrfile/update"

    invoke-static {v2}, Landroid/net/Uri;->parse(Ljava/lang/String;)Landroid/net/Uri;

    move-result-object v2

    const-string v3, "application/vnd.android.package-archive"

    invoke-virtual {v1, v2, v3}, Landroid/content/Intent;->setDataAndType(Landroid/net/Uri;Ljava/lang/String;)Landroid/content/Intent;

    const/high16 v2, 0x10000000

    const/4 v3, 0x1

    or-int/2addr v2, v3

    invoke-virtual {v1, v2}, Landroid/content/Intent;->setFlags(I)Landroid/content/Intent;

    invoke-virtual {p0, v1}, Landroid/content/Context;->startActivity(Landroid/content/Intent;)V

    return-void

    :noperm
    const/4 v1, 0x1

    sput-boolean v1, Lcom/capcom/zombiecafeandroid/UpdateChecker;->sNeedsInstallRetry:Z

    const-string v1, "Allow install permission - the update installs automatically"

    const/4 v2, 0x1

    invoke-static {p0, v1, v2}, Landroid/widget/Toast;->makeText(Landroid/content/Context;Ljava/lang/CharSequence;I)Landroid/widget/Toast;

    move-result-object v1

    invoke-virtual {v1}, Landroid/widget/Toast;->show()V

    goto :gotperm

    :done
    return-void
.end method

# Called from Activity.onResume: if the install was interrupted by the
# "install unknown apps" permission trip and the permission is now
# granted, continue installing automatically.
.method static onResume(Landroid/content/Context;)V
    .locals 1

    sget-boolean v0, Lcom/capcom/zombiecafeandroid/UpdateChecker;->sNeedsInstallRetry:Z

    if-eqz v0, :done

    invoke-static {p0}, Lcom/capcom/zombiecafeandroid/UpdateChecker;->canInstall(Landroid/content/Context;)Z

    move-result v0

    if-eqz v0, :done

    const/4 v0, 0x0

    sput-boolean v0, Lcom/capcom/zombiecafeandroid/UpdateChecker;->sNeedsInstallRetry:Z

    invoke-static {p0}, Lcom/capcom/zombiecafeandroid/UpdateChecker;->fireInstall(Landroid/content/Context;)V

    :done
    return-void
.end method

.method static writeVer(Ljava/io/File;Ljava/lang/String;)V
    .locals 3

    .catch Ljava/lang/Throwable; {:try_start .. :try_end} :catch_0

    :try_start
    new-instance v0, Ljava/io/FileOutputStream;

    invoke-direct {v0, p0}, Ljava/io/FileOutputStream;-><init>(Ljava/io/File;)V

    const-string v1, "UTF-8"

    invoke-virtual {p1, v1}, Ljava/lang/String;->getBytes(Ljava/lang/String;)[B

    move-result-object v1

    invoke-virtual {v0, v1}, Ljava/io/FileOutputStream;->write([B)V

    invoke-virtual {v0}, Ljava/io/FileOutputStream;->close()V
    :try_end
    goto :out

    :catch_0
    move-exception v0

    :out
    return-void
.end method

.method static readVer(Ljava/io/File;)Ljava/lang/String;
    .locals 3

    .catch Ljava/lang/Throwable; {:try_start .. :try_end} :catch_0

    invoke-virtual {p0}, Ljava/io/File;->exists()Z

    move-result v0

    if-eqz v0, :none

    :try_start
    new-instance v0, Ljava/io/FileInputStream;

    invoke-direct {v0, p0}, Ljava/io/FileInputStream;-><init>(Ljava/io/File;)V

    invoke-static {v0}, Lcom/capcom/zombiecafeandroid/UpdateChecker;->readAll(Ljava/io/InputStream;)[B

    move-result-object v1

    invoke-virtual {v0}, Ljava/io/FileInputStream;->close()V

    new-instance v0, Ljava/lang/String;

    const-string v2, "UTF-8"

    invoke-direct {v0, v1, v2}, Ljava/lang/String;-><init>([BLjava/lang/String;)V

    invoke-virtual {v0}, Ljava/lang/String;->trim()Ljava/lang/String;

    move-result-object v0
    :try_end
    return-object v0

    :catch_0
    move-exception v0

    :none
    const/4 v0, 0x0

    return-object v0
.end method

.method static detectMod()Z
    .locals 1

    .catch Ljava/lang/ClassNotFoundException; {:try_start .. :try_end} :catch_0

    :try_start
    const-string v0, "com.capcom.zombiecafeandroid.DebugMenuButtonListener"

    invoke-static {v0}, Ljava/lang/Class;->forName(Ljava/lang/String;)Ljava/lang/Class;
    :try_end
    const/4 v0, 0x1

    return v0

    :catch_0
    move-exception v0

    const/4 v0, 0x0

    return v0
.end method

# returns {tag_name, browser_download_url} of the newest release that ships
# an APK for this variant, or null
.method static findUpdate(Landroid/app/Activity;)[Ljava/lang/String;
    .locals 10

    .catch Ljava/lang/Throwable; {:try_start .. :try_end} :catch_0

    :try_start
    new-instance v1, Ljava/net/URL;

    const-string v2, "https://api.github.com/repos/dazoarmando29/Zombie-Cafe-Revival-Project-Updated-Phones/releases"

    invoke-direct {v1, v2}, Ljava/net/URL;-><init>(Ljava/lang/String;)V

    invoke-virtual {v1}, Ljava/net/URL;->openConnection()Ljava/net/URLConnection;

    move-result-object v1

    check-cast v1, Ljava/net/HttpURLConnection;

    const-string v2, "GET"

    invoke-virtual {v1, v2}, Ljava/net/HttpURLConnection;->setRequestMethod(Ljava/lang/String;)V

    const-string v2, "User-Agent"

    const-string v3, "ZCR-Updater"

    invoke-virtual {v1, v2, v3}, Ljava/net/HttpURLConnection;->setRequestProperty(Ljava/lang/String;Ljava/lang/String;)V

    const/16 v2, 0x3a98

    invoke-virtual {v1, v2}, Ljava/net/HttpURLConnection;->setConnectTimeout(I)V

    const/16 v2, 0x4e20

    invoke-virtual {v1, v2}, Ljava/net/HttpURLConnection;->setReadTimeout(I)V

    invoke-virtual {v1}, Ljava/net/HttpURLConnection;->getResponseCode()I

    move-result v2

    const/16 v3, 0xc8

    if-ne v2, v3, :badconn

    invoke-virtual {v1}, Ljava/net/HttpURLConnection;->getInputStream()Ljava/io/InputStream;

    move-result-object v2

    invoke-static {v2}, Lcom/capcom/zombiecafeandroid/UpdateChecker;->readAll(Ljava/io/InputStream;)[B

    move-result-object v3

    invoke-virtual {v2}, Ljava/io/InputStream;->close()V

    invoke-virtual {v1}, Ljava/net/HttpURLConnection;->disconnect()V

    new-instance v1, Ljava/lang/String;

    const-string v2, "UTF-8"

    invoke-direct {v1, v3, v2}, Ljava/lang/String;-><init>([BLjava/lang/String;)V

    new-instance v2, Lorg/json/JSONArray;

    invoke-direct {v2, v1}, Lorg/json/JSONArray;-><init>(Ljava/lang/String;)V

    invoke-static {}, Lcom/capcom/zombiecafeandroid/UpdateChecker;->detectMod()Z

    move-result v3

    const/4 v4, 0x0

    :loop_i
    invoke-virtual {v2}, Lorg/json/JSONArray;->length()I

    move-result v5

    if-ge v4, v5, :nomatch

    invoke-virtual {v2, v4}, Lorg/json/JSONArray;->getJSONObject(I)Lorg/json/JSONObject;

    move-result-object v5

    const-string v6, "assets"

    invoke-virtual {v5, v6}, Lorg/json/JSONObject;->getJSONArray(Ljava/lang/String;)Lorg/json/JSONArray;

    move-result-object v6

    const/4 v7, 0x0

    :loop_j
    invoke-virtual {v6}, Lorg/json/JSONArray;->length()I

    move-result v8

    if-ge v7, v8, :next_i

    invoke-virtual {v6, v7}, Lorg/json/JSONArray;->getJSONObject(I)Lorg/json/JSONObject;

    move-result-object v8

    const-string v9, "name"

    invoke-virtual {v8, v9}, Lorg/json/JSONObject;->getString(Ljava/lang/String;)Ljava/lang/String;

    move-result-object v9

    const-string v1, ".apk"

    invoke-virtual {v9, v1}, Ljava/lang/String;->endsWith(Ljava/lang/String;)Z

    move-result v1

    if-eqz v1, :next_j

    const-string v1, "ModMenu"

    invoke-virtual {v9, v1}, Ljava/lang/String;->indexOf(Ljava/lang/String;)I

    move-result v1

    if-ltz v1, :is_vanilla

    const/4 v1, 0x1

    goto :have_mod

    :is_vanilla
    const/4 v1, 0x0

    :have_mod
    if-eq v1, v3, :match

    :next_j
    add-int/lit8 v7, v7, 0x1

    goto :loop_j

    :match
    const-string v2, "ZCR-Updater"

    const-string v6, "asset matched"

    invoke-static {v2, v6}, Landroid/util/Log;->i(Ljava/lang/String;Ljava/lang/String;)I

    const/4 v1, 0x2

    new-array v1, v1, [Ljava/lang/String;

    const/4 v2, 0x0

    const-string v3, "tag_name"

    invoke-virtual {v5, v3}, Lorg/json/JSONObject;->getString(Ljava/lang/String;)Ljava/lang/String;

    move-result-object v3

    aput-object v3, v1, v2

    const/4 v2, 0x1

    const-string v3, "browser_download_url"

    invoke-virtual {v8, v3}, Lorg/json/JSONObject;->getString(Ljava/lang/String;)Ljava/lang/String;

    move-result-object v3

    aput-object v3, v1, v2

    :try_end
    return-object v1

    :next_i
    add-int/lit8 v4, v4, 0x1

    goto :loop_i

    :badconn
    const-string v2, "ZCR-Updater"

    const-string v3, "http not 200"

    invoke-static {v2, v3}, Landroid/util/Log;->i(Ljava/lang/String;Ljava/lang/String;)I

    invoke-virtual {v1}, Ljava/net/HttpURLConnection;->disconnect()V

    :nomatch
    const-string v1, "ZCR-Updater"

    const-string v2, "no matching asset"

    invoke-static {v1, v2}, Landroid/util/Log;->i(Ljava/lang/String;Ljava/lang/String;)I

    const/4 v1, 0x0

    return-object v1

    :catch_0
    move-exception v1

    const-string v2, "ZCR-Updater"

    const-string v3, "findUpdate failed"

    invoke-static {v2, v3, v1}, Landroid/util/Log;->e(Ljava/lang/String;Ljava/lang/String;Ljava/lang/Throwable;)I

    const/4 v1, 0x0

    return-object v1
.end method

.method static check()V
    .locals 6

    .catch Ljava/lang/Throwable; {:try_start .. :try_end} :catch_ver

    sget-object v0, Lcom/capcom/zombiecafeandroid/UpdateChecker;->sActivity:Ljava/lang/ref/WeakReference;

    if-eqz v0, :done

    invoke-virtual {v0}, Ljava/lang/ref/WeakReference;->get()Ljava/lang/Object;

    move-result-object v0

    check-cast v0, Landroid/app/Activity;

    if-eqz v0, :done

    invoke-static {v0}, Lcom/capcom/zombiecafeandroid/UpdateChecker;->findUpdate(Landroid/app/Activity;)[Ljava/lang/String;

    move-result-object v1

    if-eqz v1, :done

    const/4 v2, 0x0

    :try_start
    invoke-virtual {v0}, Landroid/app/Activity;->getPackageManager()Landroid/content/pm/PackageManager;

    move-result-object v3

    invoke-virtual {v0}, Landroid/app/Activity;->getPackageName()Ljava/lang/String;

    move-result-object v4

    const/4 v5, 0x0

    invoke-virtual {v3, v4, v5}, Landroid/content/pm/PackageManager;->getPackageInfo(Ljava/lang/String;I)Landroid/content/pm/PackageInfo;

    move-result-object v3

    iget-object v2, v3, Landroid/content/pm/PackageInfo;->versionName:Ljava/lang/String;
    :try_end
    goto :havever

    :catch_ver
    move-exception v3

    :havever
    const/4 v3, 0x0

    aget-object v3, v1, v3

    invoke-static {v2, v3}, Lcom/capcom/zombiecafeandroid/UpdateChecker;->compareVersions(Ljava/lang/String;Ljava/lang/String;)I

    move-result v2

    const-string v4, "ZCR-Updater"

    if-ltz v2, :prompt

    const-string v5, "up to date"

    invoke-static {v4, v5}, Landroid/util/Log;->i(Ljava/lang/String;Ljava/lang/String;)I

    goto :done

    :prompt
    const-string v5, "update available"

    invoke-static {v4, v5}, Landroid/util/Log;->i(Ljava/lang/String;Ljava/lang/String;)I

    const/4 v2, 0x0

    aget-object v2, v1, v2

    sput-object v2, Lcom/capcom/zombiecafeandroid/UpdateChecker;->sPendingTag:Ljava/lang/String;

    const/4 v2, 0x1

    aget-object v2, v1, v2

    sput-object v2, Lcom/capcom/zombiecafeandroid/UpdateChecker;->sPendingUrl:Ljava/lang/String;

    sget-object v2, Lcom/capcom/zombiecafeandroid/UpdateChecker;->sHandler:Landroid/os/Handler;

    if-eqz v2, :done

    const/4 v3, 0x1

    invoke-virtual {v2, v3}, Landroid/os/Handler;->sendEmptyMessage(I)Z

    :done
    return-void
.end method

# compares installed versionName against a release tag: <0 = update available
.method static compareVersions(Ljava/lang/String;Ljava/lang/String;)I
    .locals 6

    invoke-static {p0}, Lcom/capcom/zombiecafeandroid/UpdateChecker;->parseVer(Ljava/lang/String;)[I

    move-result-object v0

    invoke-static {p1}, Lcom/capcom/zombiecafeandroid/UpdateChecker;->parseVer(Ljava/lang/String;)[I

    move-result-object v1

    const/4 v2, 0x0

    :loop
    const/4 v3, 0x3

    if-ge v2, v3, :eq

    array-length v3, v0

    if-ge v2, v3, :xzero

    aget v3, v0, v2

    goto :xhave

    :xzero
    const/4 v3, 0x0

    :xhave
    array-length v4, v1

    if-ge v2, v4, :yzero

    aget v4, v1, v2

    goto :yhave

    :yzero
    const/4 v4, 0x0

    :yhave
    if-eq v3, v4, :next

    if-ge v3, v4, :gt

    const/4 v3, -0x1

    return v3

    :gt
    const/4 v3, 0x1

    return v3

    :next
    add-int/lit8 v2, v2, 0x1

    goto :loop

    :eq
    const/4 v3, 0x0

    return v3
.end method

.method static parseVer(Ljava/lang/String;)[I
    .locals 6

    if-eqz p0, :empty

    move-object v0, p0

    const-string v1, "ZCR v"

    invoke-virtual {v0, v1}, Ljava/lang/String;->indexOf(Ljava/lang/String;)I

    move-result v1

    if-ltz v1, :no_legacy

    add-int/lit8 v1, v1, 0x5

    invoke-virtual {v0, v1}, Ljava/lang/String;->substring(I)Ljava/lang/String;

    move-result-object v0

    :no_legacy
    const-string v1, "v"

    invoke-virtual {v0, v1}, Ljava/lang/String;->startsWith(Ljava/lang/String;)Z

    move-result v1

    if-eqz v1, :no_v

    const/4 v1, 0x1

    invoke-virtual {v0, v1}, Ljava/lang/String;->substring(I)Ljava/lang/String;

    move-result-object v0

    :no_v
    const/16 v1, 0x2d

    invoke-virtual {v0, v1}, Ljava/lang/String;->indexOf(I)I

    move-result v1

    if-ltz v1, :no_cut

    const/4 v2, 0x0

    invoke-virtual {v0, v2, v1}, Ljava/lang/String;->substring(II)Ljava/lang/String;

    move-result-object v0

    :no_cut
    const-string v1, "[.]"

    invoke-virtual {v0, v1}, Ljava/lang/String;->split(Ljava/lang/String;)[Ljava/lang/String;

    move-result-object v0

    array-length v1, v0

    const/4 v2, 0x3

    if-le v1, v2, :lenok

    move v1, v2

    :lenok
    new-array v2, v1, [I

    const/4 v3, 0x0

    :loop
    if-ge v3, v1, :done

    aget-object v4, v0, v3

    .catch Ljava/lang/NumberFormatException; {:try_start .. :try_end} :catch_num

    :try_start
    invoke-static {v4}, Ljava/lang/Integer;->parseInt(Ljava/lang/String;)I

    move-result v4
    :try_end
    goto :gotint

    :catch_num
    move-exception v4

    const/4 v4, 0x0

    :gotint
    aput v4, v2, v3

    add-int/lit8 v3, v3, 0x1

    goto :loop

    :empty
    const/4 v0, 0x0

    new-array v0, v0, [I

    return-object v0

    :done
    return-object v2
.end method

.method static beginDownload()V
    .locals 3

    sget-object v0, Lcom/capcom/zombiecafeandroid/UpdateChecker;->sActivity:Ljava/lang/ref/WeakReference;

    if-eqz v0, :done

    invoke-virtual {v0}, Ljava/lang/ref/WeakReference;->get()Ljava/lang/Object;

    move-result-object v0

    check-cast v0, Landroid/app/Activity;

    if-eqz v0, :done

    new-instance v1, Landroid/app/ProgressDialog;

    invoke-direct {v1, v0}, Landroid/app/ProgressDialog;-><init>(Landroid/content/Context;)V

    const-string v2, "Updating"

    invoke-virtual {v1, v2}, Landroid/app/ProgressDialog;->setTitle(Ljava/lang/CharSequence;)V

    new-instance v2, Ljava/lang/StringBuilder;

    invoke-direct {v2}, Ljava/lang/StringBuilder;-><init>()V

    const-string v0, "Downloading "

    invoke-virtual {v2, v0}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    sget-object v0, Lcom/capcom/zombiecafeandroid/UpdateChecker;->sPendingTag:Ljava/lang/String;

    invoke-virtual {v2, v0}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    const-string v0, "..."

    invoke-virtual {v2, v0}, Ljava/lang/StringBuilder;->append(Ljava/lang/String;)Ljava/lang/StringBuilder;

    invoke-virtual {v2}, Ljava/lang/StringBuilder;->toString()Ljava/lang/String;

    move-result-object v0

    invoke-virtual {v1, v0}, Landroid/app/ProgressDialog;->setMessage(Ljava/lang/CharSequence;)V

    const/4 v0, 0x1

    invoke-virtual {v1, v0}, Landroid/app/ProgressDialog;->setIndeterminate(Z)V

    const/4 v0, 0x0

    invoke-virtual {v1, v0}, Landroid/app/ProgressDialog;->setCancelable(Z)V

    invoke-virtual {v1}, Landroid/app/ProgressDialog;->show()V

    sput-object v1, Lcom/capcom/zombiecafeandroid/UpdateChecker;->sProgress:Landroid/app/ProgressDialog;

    new-instance v0, Lcom/capcom/zombiecafeandroid/UpdateChecker$1;

    const/4 v1, 0x1

    invoke-direct {v0, v1}, Lcom/capcom/zombiecafeandroid/UpdateChecker$1;-><init>(I)V

    invoke-virtual {v0}, Ljava/lang/Thread;->start()V

    :done
    return-void
.end method

.method static doDownload()V
    .locals 5

    .catch Ljava/lang/Throwable; {:try_start .. :try_end} :catch_0

    sget-object v0, Lcom/capcom/zombiecafeandroid/UpdateChecker;->sActivity:Ljava/lang/ref/WeakReference;

    if-eqz v0, :done

    invoke-virtual {v0}, Ljava/lang/ref/WeakReference;->get()Ljava/lang/Object;

    move-result-object v0

    check-cast v0, Landroid/app/Activity;

    if-eqz v0, :done

    invoke-static {v0}, Lcom/capcom/zombiecafeandroid/UpdateChecker;->resolveApkFile(Landroid/content/Context;)Ljava/io/File;

    move-result-object v0

    :try_start
    new-instance v1, Ljava/net/URL;

    sget-object v2, Lcom/capcom/zombiecafeandroid/UpdateChecker;->sPendingUrl:Ljava/lang/String;

    invoke-direct {v1, v2}, Ljava/net/URL;-><init>(Ljava/lang/String;)V

    invoke-virtual {v1}, Ljava/net/URL;->openConnection()Ljava/net/URLConnection;

    move-result-object v1

    check-cast v1, Ljava/net/HttpURLConnection;

    const-string v2, "GET"

    invoke-virtual {v1, v2}, Ljava/net/HttpURLConnection;->setRequestMethod(Ljava/lang/String;)V

    const/4 v2, 0x1

    invoke-virtual {v1, v2}, Ljava/net/HttpURLConnection;->setInstanceFollowRedirects(Z)V

    const/16 v2, 0x3a98

    invoke-virtual {v1, v2}, Ljava/net/HttpURLConnection;->setConnectTimeout(I)V

    const v2, 0x1d4c0

    invoke-virtual {v1, v2}, Ljava/net/HttpURLConnection;->setReadTimeout(I)V

    invoke-virtual {v1}, Ljava/net/HttpURLConnection;->getResponseCode()I

    move-result v2

    const/16 v3, 0xc8

    if-ne v2, v3, :badconn

    invoke-virtual {v1}, Ljava/net/HttpURLConnection;->getInputStream()Ljava/io/InputStream;

    move-result-object v2

    new-instance v3, Ljava/io/FileOutputStream;

    invoke-direct {v3, v0}, Ljava/io/FileOutputStream;-><init>(Ljava/io/File;)V

    invoke-static {v2, v3}, Lcom/capcom/zombiecafeandroid/UpdateChecker;->copyStream(Ljava/io/InputStream;Ljava/io/OutputStream;)V

    invoke-virtual {v3}, Ljava/io/FileOutputStream;->close()V

    invoke-virtual {v2}, Ljava/io/InputStream;->close()V

    invoke-virtual {v1}, Ljava/net/HttpURLConnection;->disconnect()V

    new-instance v2, Ljava/io/File;

    invoke-virtual {v0}, Ljava/io/File;->getParentFile()Ljava/io/File;

    move-result-object v3

    const-string v4, "update.version"

    invoke-direct {v2, v3, v4}, Ljava/io/File;-><init>(Ljava/io/File;Ljava/lang/String;)V

    sget-object v3, Lcom/capcom/zombiecafeandroid/UpdateChecker;->sPendingTag:Ljava/lang/String;

    invoke-static {v2, v3}, Lcom/capcom/zombiecafeandroid/UpdateChecker;->writeVer(Ljava/io/File;Ljava/lang/String;)V

    sget-object v1, Lcom/capcom/zombiecafeandroid/UpdateChecker;->sHandler:Landroid/os/Handler;

    const/4 v2, 0x3

    invoke-virtual {v1, v2}, Landroid/os/Handler;->sendEmptyMessage(I)Z
    :try_end
    goto :done

    :catch_0
    move-exception v1

    if-eqz v0, :failmsg

    invoke-virtual {v0}, Ljava/io/File;->delete()Z

    :failmsg
    sget-object v1, Lcom/capcom/zombiecafeandroid/UpdateChecker;->sHandler:Landroid/os/Handler;

    if-eqz v1, :done

    const/4 v2, 0x4

    invoke-virtual {v1, v2}, Landroid/os/Handler;->sendEmptyMessage(I)Z

    goto :done

    :badconn
    invoke-virtual {v1}, Ljava/net/HttpURLConnection;->disconnect()V

    sget-object v1, Lcom/capcom/zombiecafeandroid/UpdateChecker;->sHandler:Landroid/os/Handler;

    if-eqz v1, :done

    const/4 v2, 0x4

    invoke-virtual {v1, v2}, Landroid/os/Handler;->sendEmptyMessage(I)Z

    :done
    return-void
.end method

.method static readAll(Ljava/io/InputStream;)[B
    .locals 4

    new-instance v0, Ljava/io/ByteArrayOutputStream;

    invoke-direct {v0}, Ljava/io/ByteArrayOutputStream;-><init>()V

    const/16 v1, 0x1000

    new-array v1, v1, [B

    :loop
    invoke-virtual {p0, v1}, Ljava/io/InputStream;->read([B)I

    move-result v2

    if-lez v2, :done

    const/4 v3, 0x0

    invoke-virtual {v0, v1, v3, v2}, Ljava/io/ByteArrayOutputStream;->write([BII)V

    goto :loop

    :done
    invoke-virtual {v0}, Ljava/io/ByteArrayOutputStream;->toByteArray()[B

    move-result-object v0

    return-object v0
.end method

.method static copyStream(Ljava/io/InputStream;Ljava/io/OutputStream;)V
    .locals 4

    const/16 v0, 0x4000

    new-array v0, v0, [B

    :loop
    invoke-virtual {p0, v0}, Ljava/io/InputStream;->read([B)I

    move-result v1

    if-lez v1, :done

    const/4 v2, 0x0

    invoke-virtual {p1, v0, v2, v1}, Ljava/io/OutputStream;->write([BII)V

    goto :loop

    :done
    return-void
.end method
