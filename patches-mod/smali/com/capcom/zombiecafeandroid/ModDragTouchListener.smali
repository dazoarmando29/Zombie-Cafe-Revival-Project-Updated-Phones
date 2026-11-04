.class public Lcom/capcom/zombiecafeandroid/ModDragTouchListener;
.super Ljava/lang/Object;
.implements Landroid/view/View$OnTouchListener;

# Drag-to-move + tap-to-click for the MOD trigger button (ZCR).
#
# The mod redesign pinned a 48dp button to the bottom-right corner, right
# on top of the game's back button. This listener lets the user drag the
# trigger anywhere on screen; a tap (movement below the system touch slop)
# still fires the button's OnClickListener via performClick(), which
# opens/closes the mod panel.
#
# Register discipline (keeps ART's verifier happy): v0/v2/v3 ints,
# v1 object scratch, v4..v8 floats. Never move across type boundaries.

.field private downX:F
.field private downY:F
.field private startX:F
.field private startY:F
.field private dragging:Z
.field private slop:F

.method public constructor <init>()V
    .locals 1

    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    const/high16 v0, 0x42000000    # 32f fallback slop

    iput v0, p0, Lcom/capcom/zombiecafeandroid/ModDragTouchListener;->slop:F

    return-void
.end method

.method public onTouch(Landroid/view/View;Landroid/view/MotionEvent;)Z
    .locals 9

    invoke-virtual {p2}, Landroid/view/MotionEvent;->getActionMasked()I

    move-result v0

    if-nez v0, :not_down

    # ---- ACTION_DOWN: record anchor (v6..v8 floats) ----
    invoke-virtual {p2}, Landroid/view/MotionEvent;->getRawX()F

    move-result v6

    iput v6, p0, Lcom/capcom/zombiecafeandroid/ModDragTouchListener;->downX:F

    invoke-virtual {p2}, Landroid/view/MotionEvent;->getRawY()F

    move-result v6

    iput v6, p0, Lcom/capcom/zombiecafeandroid/ModDragTouchListener;->downY:F

    invoke-virtual {p1}, Landroid/view/View;->getX()F

    move-result v6

    iput v6, p0, Lcom/capcom/zombiecafeandroid/ModDragTouchListener;->startX:F

    invoke-virtual {p1}, Landroid/view/View;->getY()F

    move-result v6

    iput v6, p0, Lcom/capcom/zombiecafeandroid/ModDragTouchListener;->startY:F

    const/4 v2, 0x0

    iput-boolean v2, p0, Lcom/capcom/zombiecafeandroid/ModDragTouchListener;->dragging:Z

    invoke-virtual {p1}, Landroid/view/View;->getContext()Landroid/content/Context;

    move-result-object v1

    invoke-static {v1}, Landroid/view/ViewConfiguration;->get(Landroid/content/Context;)Landroid/view/ViewConfiguration;

    move-result-object v1

    invoke-virtual {v1}, Landroid/view/ViewConfiguration;->getScaledTouchSlop()I

    move-result v2

    int-to-float v6, v2

    iput v6, p0, Lcom/capcom/zombiecafeandroid/ModDragTouchListener;->slop:F

    const/4 v2, 0x1

    return v2

    :not_down
    const/4 v2, 0x1

    if-ne v0, v2, :not_up

    # ---- ACTION_UP: tap -> click ----
    iget-boolean v2, p0, Lcom/capcom/zombiecafeandroid/ModDragTouchListener;->dragging:Z

    if-nez v2, :up_done

    invoke-virtual {p1}, Landroid/view/View;->performClick()Z

    :up_done
    const/4 v2, 0x0

    iput-boolean v2, p0, Lcom/capcom/zombiecafeandroid/ModDragTouchListener;->dragging:Z

    const/4 v2, 0x1

    return v2

    :not_up
    const/4 v3, 0x2

    if-ne v0, v3, :other

    # ---- ACTION_MOVE (v4=dx, v5=dy floats) ----
    invoke-virtual {p2}, Landroid/view/MotionEvent;->getRawX()F

    move-result v4

    iget v6, p0, Lcom/capcom/zombiecafeandroid/ModDragTouchListener;->downX:F

    sub-float/2addr v4, v6

    invoke-virtual {p2}, Landroid/view/MotionEvent;->getRawY()F

    move-result v5

    iget v6, p0, Lcom/capcom/zombiecafeandroid/ModDragTouchListener;->downY:F

    sub-float/2addr v5, v6

    iget-boolean v2, p0, Lcom/capcom/zombiecafeandroid/ModDragTouchListener;->dragging:Z

    if-nez v2, :do_drag

    # start dragging once the finger moved past the touch slop
    # (was if-lez: INVERTED - taps were classified as drags, so
    # performClick never fired and the mod menu never opened)
    invoke-static {v4}, Ljava/lang/Math;->abs(F)F

    move-result v6

    iget v7, p0, Lcom/capcom/zombiecafeandroid/ModDragTouchListener;->slop:F

    cmpl-float v6, v6, v7

    if-gtz v6, :set_drag

    invoke-static {v5}, Ljava/lang/Math;->abs(F)F

    move-result v6

    cmpl-float v6, v6, v7

    if-gtz v6, :set_drag

    goto :do_drag

    :set_drag
    const/4 v2, 0x1

    iput-boolean v2, p0, Lcom/capcom/zombiecafeandroid/ModDragTouchListener;->dragging:Z

    :do_drag
    iget-boolean v2, p0, Lcom/capcom/zombiecafeandroid/ModDragTouchListener;->dragging:Z

    if-eqz v2, :other

    # v6 = clamp(startX + dx, 0, screenW - width)
    iget v6, p0, Lcom/capcom/zombiecafeandroid/ModDragTouchListener;->startX:F

    add-float/2addr v6, v4

    # v7 = clamp(startY + dy, 0, screenH - height)
    iget v7, p0, Lcom/capcom/zombiecafeandroid/ModDragTouchListener;->startY:F

    add-float/2addr v7, v5

    invoke-virtual {p1}, Landroid/view/View;->getResources()Landroid/content/res/Resources;

    move-result-object v1

    invoke-virtual {v1}, Landroid/content/res/Resources;->getDisplayMetrics()Landroid/util/DisplayMetrics;

    move-result-object v1

    iget v2, v1, Landroid/util/DisplayMetrics;->widthPixels:I

    invoke-virtual {p1}, Landroid/view/View;->getWidth()I

    move-result v3

    sub-int/2addr v2, v3

    int-to-float v8, v2        # v8 = maxX

    iget v2, v1, Landroid/util/DisplayMetrics;->heightPixels:I

    invoke-virtual {p1}, Landroid/view/View;->getHeight()I

    move-result v3

    sub-int/2addr v2, v3

    int-to-float v5, v2        # v5 = maxY (dy no longer needed)

    const/4 v2, 0x0

    int-to-float v3, v2        # v3 = 0.0f

    # clamp x
    cmpl-float v2, v6, v3

    if-gez v2, :x_hi

    move v6, v3

    goto :x_clamped

    :x_hi
    cmpl-float v2, v6, v8

    if-lez v2, :x_clamped

    move v6, v8

    :x_clamped
    # clamp y
    cmpl-float v2, v7, v3

    if-gez v2, :y_hi

    move v7, v3

    goto :y_clamped

    :y_hi
    cmpl-float v2, v7, v5

    if-lez v2, :y_clamped

    move v7, v5

    :y_clamped
    invoke-virtual {p1, v6}, Landroid/view/View;->setX(F)V

    invoke-virtual {p1, v7}, Landroid/view/View;->setY(F)V

    :other
    const/4 v2, 0x1

    return v2
.end method
