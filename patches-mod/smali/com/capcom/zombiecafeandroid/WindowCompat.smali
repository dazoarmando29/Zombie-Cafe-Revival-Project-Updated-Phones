.class public final Lcom/capcom/zombiecafeandroid/WindowCompat;
.super Ljava/lang/Object;

# ZCR: window compatibility - keep the screen on while the cafe runs and
# render into the display-cutout (notch) area. MUST be called on the UI
# thread (Activity.onCreate): setAttributes() touches the view hierarchy
# and throws CalledFromWrongThreadException from the GL thread.
# (2026-11-03: the first attempt did this from JNI on the GLThread and
# only addFlags worked.)

.method public static apply(Landroid/view/Window;)V
    .locals 3

    if-eqz p0, :end

    # FLAG_KEEP_SCREEN_ON = 0x80
    const/16 v0, 0x80

    invoke-virtual {p0, v0}, Landroid/view/Window;->addFlags(I)V

    # LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES = 1, API 27+
    sget v0, Landroid/os/Build$VERSION;->SDK_INT:I

    const/16 v1, 0x1b

    if-lt v0, v1, :end

    invoke-virtual {p0}, Landroid/view/Window;->getAttributes()Landroid/view/WindowManager$LayoutParams;

    move-result-object v0

    const/4 v1, 0x1

    iput v1, v0, Landroid/view/WindowManager$LayoutParams;->layoutInDisplayCutoutMode:I

    invoke-virtual {p0, v0}, Landroid/view/Window;->setAttributes(Landroid/view/WindowManager$LayoutParams;)V

    :end
    return-void
.end method
