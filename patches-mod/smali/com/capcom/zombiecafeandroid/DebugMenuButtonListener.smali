.class public final Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;
.super Ljava/lang/Object;
.implements Landroid/view/View$OnClickListener;


# direct methods
.method public constructor <init>()V
    .locals 0

    invoke-direct {p0}, Ljava/lang/Object;-><init>()V

    return-void
.end method

.method private static dp(Landroid/content/Context;I)I
    .locals 2

    invoke-virtual {p0}, Landroid/content/Context;->getResources()Landroid/content/res/Resources;

    move-result-object v0

    invoke-virtual {v0}, Landroid/content/res/Resources;->getDisplayMetrics()Landroid/util/DisplayMetrics;

    move-result-object v0

    iget v0, v0, Landroid/util/DisplayMetrics;->density:F

    int-to-float v1, p1

    mul-float/2addr v0, v1

    float-to-int v0, v0

    return v0
.end method

.method private static panelBg(Landroid/content/Context;)Landroid/graphics/drawable/GradientDrawable;
    .locals 6

    new-instance v0, Landroid/graphics/drawable/GradientDrawable;

    invoke-direct {v0}, Landroid/graphics/drawable/GradientDrawable;-><init>()V

    const/4 v1, 0x2

    new-array v1, v1, [I

    const/4 v2, 0x0

    const-string v3, "#2A2018"

    invoke-static {v3}, Landroid/graphics/Color;->parseColor(Ljava/lang/String;)I

    move-result v3

    aput v3, v1, v2

    const/4 v2, 0x1

    const-string v3, "#16100B"

    invoke-static {v3}, Landroid/graphics/Color;->parseColor(Ljava/lang/String;)I

    move-result v3

    aput v3, v1, v2

    invoke-virtual {v0, v1}, Landroid/graphics/drawable/GradientDrawable;->setColors([I)V

    sget-object v1, Landroid/graphics/drawable/GradientDrawable$Orientation;->TOP_BOTTOM:Landroid/graphics/drawable/GradientDrawable$Orientation;

    invoke-virtual {v0, v1}, Landroid/graphics/drawable/GradientDrawable;->setOrientation(Landroid/graphics/drawable/GradientDrawable$Orientation;)V

    const/16 v1, 0x12

    invoke-static {p0, v1}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->dp(Landroid/content/Context;I)I

    move-result v1

    int-to-float v1, v1

    invoke-virtual {v0, v1}, Landroid/graphics/drawable/GradientDrawable;->setCornerRadius(F)V

    const/4 v1, 0x2

    invoke-static {p0, v1}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->dp(Landroid/content/Context;I)I

    move-result v1

    const-string v2, "#6E5B2C"

    invoke-static {v2}, Landroid/graphics/Color;->parseColor(Ljava/lang/String;)I

    move-result v2

    invoke-virtual {v0, v1, v2}, Landroid/graphics/drawable/GradientDrawable;->setStroke(II)V

    return-object v0
.end method

.method private static btnBg(Landroid/content/Context;Z)Landroid/graphics/drawable/StateListDrawable;
    .locals 7

    new-instance v0, Landroid/graphics/drawable/GradientDrawable;

    invoke-direct {v0}, Landroid/graphics/drawable/GradientDrawable;-><init>()V

    const/4 v1, 0x2

    new-array v1, v1, [I

    const/4 v2, 0x0

    const-string v3, "#5A1712"

    invoke-static {v3}, Landroid/graphics/Color;->parseColor(Ljava/lang/String;)I

    move-result v3

    aput v3, v1, v2

    const/4 v2, 0x1

    const-string v3, "#33100C"

    invoke-static {v3}, Landroid/graphics/Color;->parseColor(Ljava/lang/String;)I

    move-result v3

    aput v3, v1, v2

    invoke-virtual {v0, v1}, Landroid/graphics/drawable/GradientDrawable;->setColors([I)V

    sget-object v1, Landroid/graphics/drawable/GradientDrawable$Orientation;->TOP_BOTTOM:Landroid/graphics/drawable/GradientDrawable$Orientation;

    invoke-virtual {v0, v1}, Landroid/graphics/drawable/GradientDrawable;->setOrientation(Landroid/graphics/drawable/GradientDrawable$Orientation;)V

    const/16 v1, 0x8

    invoke-static {p0, v1}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->dp(Landroid/content/Context;I)I

    move-result v1

    int-to-float v1, v1

    invoke-virtual {v0, v1}, Landroid/graphics/drawable/GradientDrawable;->setCornerRadius(F)V

    const/4 v1, 0x2

    invoke-static {p0, v1}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->dp(Landroid/content/Context;I)I

    move-result v1

    if-eqz p1, :cond_nstroke

    const-string v2, "#86F04E"

    goto :goto_nstroke

    :cond_nstroke
    const-string v2, "#8B2E22"

    :goto_nstroke
    invoke-static {v2}, Landroid/graphics/Color;->parseColor(Ljava/lang/String;)I

    move-result v2

    invoke-virtual {v0, v1, v2}, Landroid/graphics/drawable/GradientDrawable;->setStroke(II)V

    new-instance v3, Landroid/graphics/drawable/GradientDrawable;

    invoke-direct {v3}, Landroid/graphics/drawable/GradientDrawable;-><init>()V

    const/4 v1, 0x2

    new-array v1, v1, [I

    const/4 v2, 0x0

    const-string v4, "#71211A"

    invoke-static {v4}, Landroid/graphics/Color;->parseColor(Ljava/lang/String;)I

    move-result v4

    aput v4, v1, v2

    const/4 v2, 0x1

    const-string v4, "#421510"

    invoke-static {v4}, Landroid/graphics/Color;->parseColor(Ljava/lang/String;)I

    move-result v4

    aput v4, v1, v2

    invoke-virtual {v3, v1}, Landroid/graphics/drawable/GradientDrawable;->setColors([I)V

    sget-object v1, Landroid/graphics/drawable/GradientDrawable$Orientation;->TOP_BOTTOM:Landroid/graphics/drawable/GradientDrawable$Orientation;

    invoke-virtual {v3, v1}, Landroid/graphics/drawable/GradientDrawable;->setOrientation(Landroid/graphics/drawable/GradientDrawable$Orientation;)V

    const/16 v1, 0x8

    invoke-static {p0, v1}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->dp(Landroid/content/Context;I)I

    move-result v1

    int-to-float v1, v1

    invoke-virtual {v3, v1}, Landroid/graphics/drawable/GradientDrawable;->setCornerRadius(F)V

    const/4 v1, 0x2

    invoke-static {p0, v1}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->dp(Landroid/content/Context;I)I

    move-result v1

    if-eqz p1, :cond_pstroke

    const-string v2, "#C7FF9E"

    goto :goto_pstroke

    :cond_pstroke
    const-string v2, "#C9705E"

    :goto_pstroke
    invoke-static {v2}, Landroid/graphics/Color;->parseColor(Ljava/lang/String;)I

    move-result v2

    invoke-virtual {v3, v1, v2}, Landroid/graphics/drawable/GradientDrawable;->setStroke(II)V

    new-instance v4, Landroid/graphics/drawable/StateListDrawable;

    invoke-direct {v4}, Landroid/graphics/drawable/StateListDrawable;-><init>()V

    const/4 v1, 0x1

    new-array v5, v1, [I

    const/4 v2, 0x0

    const v6, 0x10100a7

    aput v6, v5, v2

    invoke-virtual {v4, v5, v3}, Landroid/graphics/drawable/StateListDrawable;->addState([ILandroid/graphics/drawable/Drawable;)V

    const/4 v1, 0x0

    new-array v5, v1, [I

    invoke-virtual {v4, v5, v0}, Landroid/graphics/drawable/StateListDrawable;->addState([ILandroid/graphics/drawable/Drawable;)V

    return-object v4
.end method

.method private static txt(Landroid/content/Context;Ljava/lang/String;ILjava/lang/String;I)Landroid/widget/TextView;
    .locals 2

    new-instance v0, Landroid/widget/TextView;

    invoke-direct {v0, p0}, Landroid/widget/TextView;-><init>(Landroid/content/Context;)V

    invoke-virtual {v0, p1}, Landroid/widget/TextView;->setText(Ljava/lang/CharSequence;)V

    int-to-float v1, p2

    invoke-virtual {v0, v1}, Landroid/widget/TextView;->setTextSize(F)V

    invoke-static {p3}, Landroid/graphics/Color;->parseColor(Ljava/lang/String;)I

    move-result v1

    invoke-virtual {v0, v1}, Landroid/widget/TextView;->setTextColor(I)V

    invoke-virtual {v0, p4}, Landroid/widget/TextView;->setGravity(I)V

    return-object v0
.end method

.method private static hdr(Landroid/content/Context;Ljava/lang/String;)Landroid/widget/TextView;
    .locals 3

    const/16 v0, 0xc

    const-string v1, "#D9C9A3"

    const/4 v2, 0x3

    invoke-static {p0, p1, v0, v1, v2}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->txt(Landroid/content/Context;Ljava/lang/String;ILjava/lang/String;I)Landroid/widget/TextView;

    move-result-object v0

    sget-object v1, Landroid/graphics/Typeface;->DEFAULT_BOLD:Landroid/graphics/Typeface;

    invoke-virtual {v0, v1}, Landroid/widget/TextView;->setTypeface(Landroid/graphics/Typeface;)V

    return-object v0
.end method

.method private static row(Landroid/content/Context;)Landroid/widget/LinearLayout;
    .locals 3

    new-instance v0, Landroid/widget/LinearLayout;

    invoke-direct {v0, p0}, Landroid/widget/LinearLayout;-><init>(Landroid/content/Context;)V

    const/4 v1, 0x0

    invoke-virtual {v0, v1}, Landroid/widget/LinearLayout;->setOrientation(I)V

    const/4 v2, 0x4

    invoke-static {p0, v2}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->dp(Landroid/content/Context;I)I

    move-result v2

    invoke-virtual {v0, v1, v2, v1, v2}, Landroid/widget/LinearLayout;->setPadding(IIII)V

    return-object v0
.end method

.method private static cell(Landroid/content/Context;)Landroid/widget/LinearLayout$LayoutParams;
    .locals 4

    new-instance v0, Landroid/widget/LinearLayout$LayoutParams;

    const/4 v1, 0x0

    const/16 v2, 0x2c

    invoke-static {p0, v2}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->dp(Landroid/content/Context;I)I

    move-result v2

    const/high16 v3, 0x3f800000

    invoke-direct {v0, v1, v2, v3}, Landroid/widget/LinearLayout$LayoutParams;-><init>(IIF)V

    return-object v0
.end method

.method private static div(Landroid/content/Context;)Landroid/view/View;
    .locals 4

    new-instance v0, Landroid/view/View;

    invoke-direct {v0, p0}, Landroid/view/View;-><init>(Landroid/content/Context;)V

    new-instance v1, Landroid/graphics/drawable/GradientDrawable;

    invoke-direct {v1}, Landroid/graphics/drawable/GradientDrawable;-><init>()V

    const-string v2, "#4A3D22"

    invoke-static {v2}, Landroid/graphics/Color;->parseColor(Ljava/lang/String;)I

    move-result v2

    invoke-virtual {v1, v2}, Landroid/graphics/drawable/GradientDrawable;->setColor(I)V

    const/4 v2, 0x1

    invoke-static {p0, v2}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->dp(Landroid/content/Context;I)I

    move-result v2

    int-to-float v2, v2

    invoke-virtual {v1, v2}, Landroid/graphics/drawable/GradientDrawable;->setCornerRadius(F)V

    invoke-virtual {v0, v1}, Landroid/view/View;->setBackground(Landroid/graphics/drawable/Drawable;)V

    return-object v0
.end method

.method private static btn(Landroid/content/Context;Ljava/lang/String;IZ)Landroid/widget/Button;
    .locals 3

    new-instance v0, Landroid/widget/Button;

    invoke-direct {v0, p0}, Landroid/widget/Button;-><init>(Landroid/content/Context;)V

    const/4 v1, 0x0

    invoke-virtual {v0, v1}, Landroid/widget/Button;->setAllCaps(Z)V

    invoke-virtual {v0, p1}, Landroid/widget/Button;->setText(Ljava/lang/CharSequence;)V

    const/16 v1, 0xd

    int-to-float v1, v1

    invoke-virtual {v0, v1}, Landroid/widget/Button;->setTextSize(F)V

    if-eqz p3, :cond_color

    const-string v1, "#86F04E"

    goto :goto_color

    :cond_color
    const-string v1, "#F2E8D0"

    :goto_color
    invoke-static {v1}, Landroid/graphics/Color;->parseColor(Ljava/lang/String;)I

    move-result v1

    invoke-virtual {v0, v1}, Landroid/widget/Button;->setTextColor(I)V

    sget-object v1, Landroid/graphics/Typeface;->DEFAULT_BOLD:Landroid/graphics/Typeface;

    invoke-virtual {v0, v1}, Landroid/widget/Button;->setTypeface(Landroid/graphics/Typeface;)V

    const/16 v1, 0x11

    invoke-virtual {v0, v1}, Landroid/widget/Button;->setGravity(I)V

    invoke-static {p0, p3}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->btnBg(Landroid/content/Context;Z)Landroid/graphics/drawable/StateListDrawable;

    move-result-object v1

    invoke-virtual {v0, v1}, Landroid/widget/Button;->setBackground(Landroid/graphics/drawable/Drawable;)V

    new-instance v1, Lcom/capcom/zombiecafeandroid/DebugMenuItemClickListener;

    invoke-direct {v1, p2}, Lcom/capcom/zombiecafeandroid/DebugMenuItemClickListener;-><init>(I)V

    invoke-virtual {v0, v1}, Landroid/widget/Button;->setOnClickListener(Landroid/view/View$OnClickListener;)V

    return-object v0
.end method


# virtual methods
.method public onClick(Landroid/view/View;)V
    .locals 12

    invoke-virtual {p1}, Landroid/view/View;->getContext()Landroid/content/Context;

    move-result-object v0

    new-instance v1, Landroid/widget/LinearLayout;

    invoke-direct {v1, v0}, Landroid/widget/LinearLayout;-><init>(Landroid/content/Context;)V

    const/4 v2, 0x1

    invoke-virtual {v1, v2}, Landroid/widget/LinearLayout;->setOrientation(I)V

    invoke-static {v0}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->panelBg(Landroid/content/Context;)Landroid/graphics/drawable/GradientDrawable;

    move-result-object v2

    invoke-virtual {v1, v2}, Landroid/widget/LinearLayout;->setBackground(Landroid/graphics/drawable/Drawable;)V

    const/16 v2, 0x12

    invoke-static {v0, v2}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->dp(Landroid/content/Context;I)I

    move-result v2

    const/16 v3, 0xe

    invoke-static {v0, v3}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->dp(Landroid/content/Context;I)I

    move-result v3

    const/16 v4, 0x10

    invoke-static {v0, v4}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->dp(Landroid/content/Context;I)I

    move-result v4

    invoke-virtual {v1, v2, v3, v2, v4}, Landroid/widget/LinearLayout;->setPadding(IIII)V

    const-string v2, "MOD MENU"

    const/16 v3, 0x14

    const-string v4, "#86F04E"

    const/16 v5, 0x11

    invoke-static {v0, v2, v3, v4, v5}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->txt(Landroid/content/Context;Ljava/lang/String;ILjava/lang/String;I)Landroid/widget/TextView;

    move-result-object v2

    sget-object v3, Landroid/graphics/Typeface;->DEFAULT_BOLD:Landroid/graphics/Typeface;

    invoke-virtual {v2, v3}, Landroid/widget/TextView;->setTypeface(Landroid/graphics/Typeface;)V

    invoke-virtual {v1, v2}, Landroid/widget/LinearLayout;->addView(Landroid/view/View;)V

    const-string v2, "ZOMBIE CAFE \u00b7 CHEATS"

    const/16 v3, 0xa

    const-string v4, "#B0A184"

    const/16 v5, 0x11

    invoke-static {v0, v2, v3, v4, v5}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->txt(Landroid/content/Context;Ljava/lang/String;ILjava/lang/String;I)Landroid/widget/TextView;

    move-result-object v2

    invoke-virtual {v1, v2}, Landroid/widget/LinearLayout;->addView(Landroid/view/View;)V


    const-string v2, "ZOMBIE PERKS"

    invoke-static {v0, v2}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->hdr(Landroid/content/Context;Ljava/lang/String;)Landroid/widget/TextView;

    move-result-object v2

    invoke-virtual {v1, v2}, Landroid/widget/LinearLayout;->addView(Landroid/view/View;)V

    invoke-static {v0}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->row(Landroid/content/Context;)Landroid/widget/LinearLayout;

    move-result-object v2

    const-string v3, "VIEW PERKS & ABILITIES"

    const/4 v4, 0x7

    const/4 v5, 0x1

    invoke-static {v0, v3, v4, v5}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->btn(Landroid/content/Context;Ljava/lang/String;IZ)Landroid/widget/Button;

    move-result-object v3

    invoke-static {v0}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->cell(Landroid/content/Context;)Landroid/widget/LinearLayout$LayoutParams;

    move-result-object v4

    invoke-virtual {v2, v3, v4}, Landroid/widget/LinearLayout;->addView(Landroid/view/View;Landroid/view/ViewGroup$LayoutParams;)V

    invoke-virtual {v1, v2}, Landroid/widget/LinearLayout;->addView(Landroid/view/View;)V

    const-string v2, "RAID BOSS"

    invoke-static {v0, v2}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->hdr(Landroid/content/Context;Ljava/lang/String;)Landroid/widget/TextView;

    move-result-object v2

    invoke-virtual {v1, v2}, Landroid/widget/LinearLayout;->addView(Landroid/view/View;)V

    invoke-static {v0}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->row(Landroid/content/Context;)Landroid/widget/LinearLayout;

    move-result-object v2

    const-string v3, "BOSS STATUS & REWARDS"

    const/16 v4, 0x8

    const/4 v5, 0x1

    invoke-static {v0, v3, v4, v5}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->btn(Landroid/content/Context;Ljava/lang/String;IZ)Landroid/widget/Button;

    move-result-object v3

    invoke-static {v0}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->cell(Landroid/content/Context;)Landroid/widget/LinearLayout$LayoutParams;

    move-result-object v4

    invoke-virtual {v2, v3, v4}, Landroid/widget/LinearLayout;->addView(Landroid/view/View;Landroid/view/ViewGroup$LayoutParams;)V

    invoke-virtual {v1, v2}, Landroid/widget/LinearLayout;->addView(Landroid/view/View;)V
    invoke-static {v0}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->div(Landroid/content/Context;)Landroid/view/View;

    move-result-object v2

    new-instance v3, Landroid/widget/LinearLayout$LayoutParams;

    const/4 v4, -0x1

    const/4 v5, 0x2

    invoke-static {v0, v5}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->dp(Landroid/content/Context;I)I

    move-result v5

    invoke-direct {v3, v4, v5}, Landroid/widget/LinearLayout$LayoutParams;-><init>(II)V

    const/4 v4, 0x0

    const/16 v5, 0x8

    invoke-static {v0, v5}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->dp(Landroid/content/Context;I)I

    move-result v5

    invoke-virtual {v3, v4, v5, v4, v5}, Landroid/widget/LinearLayout$LayoutParams;->setMargins(IIII)V

    invoke-virtual {v1, v2, v3}, Landroid/widget/LinearLayout;->addView(Landroid/view/View;Landroid/view/ViewGroup$LayoutParams;)V

    const-string v2, "TOXIN"

    invoke-static {v0, v2}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->hdr(Landroid/content/Context;Ljava/lang/String;)Landroid/widget/TextView;

    move-result-object v2

    invoke-virtual {v1, v2}, Landroid/widget/LinearLayout;->addView(Landroid/view/View;)V

    invoke-static {v0}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->row(Landroid/content/Context;)Landroid/widget/LinearLayout;

    move-result-object v2

    const-string v3, "+1"

    const/4 v4, 0x0

    const/4 v5, 0x0

    invoke-static {v0, v3, v4, v5}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->btn(Landroid/content/Context;Ljava/lang/String;IZ)Landroid/widget/Button;

    move-result-object v3

    invoke-static {v0}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->cell(Landroid/content/Context;)Landroid/widget/LinearLayout$LayoutParams;

    move-result-object v4

    invoke-virtual {v2, v3, v4}, Landroid/widget/LinearLayout;->addView(Landroid/view/View;Landroid/view/ViewGroup$LayoutParams;)V

    const-string v3, "MAX"

    const/4 v4, 0x1

    const/4 v5, 0x1

    invoke-static {v0, v3, v4, v5}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->btn(Landroid/content/Context;Ljava/lang/String;IZ)Landroid/widget/Button;

    move-result-object v3

    invoke-static {v0}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->cell(Landroid/content/Context;)Landroid/widget/LinearLayout$LayoutParams;

    move-result-object v4

    invoke-virtual {v2, v3, v4}, Landroid/widget/LinearLayout;->addView(Landroid/view/View;Landroid/view/ViewGroup$LayoutParams;)V

    invoke-virtual {v1, v2}, Landroid/widget/LinearLayout;->addView(Landroid/view/View;)V

    const-string v2, "MONEY"

    invoke-static {v0, v2}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->hdr(Landroid/content/Context;Ljava/lang/String;)Landroid/widget/TextView;

    move-result-object v2

    invoke-virtual {v1, v2}, Landroid/widget/LinearLayout;->addView(Landroid/view/View;)V

    invoke-static {v0}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->row(Landroid/content/Context;)Landroid/widget/LinearLayout;

    move-result-object v2

    const-string v3, "+1"

    const/4 v4, 0x2

    const/4 v5, 0x0

    invoke-static {v0, v3, v4, v5}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->btn(Landroid/content/Context;Ljava/lang/String;IZ)Landroid/widget/Button;

    move-result-object v3

    invoke-static {v0}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->cell(Landroid/content/Context;)Landroid/widget/LinearLayout$LayoutParams;

    move-result-object v4

    invoke-virtual {v2, v3, v4}, Landroid/widget/LinearLayout;->addView(Landroid/view/View;Landroid/view/ViewGroup$LayoutParams;)V

    const-string v3, "+10K"

    const/4 v4, 0x3

    const/4 v5, 0x0

    invoke-static {v0, v3, v4, v5}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->btn(Landroid/content/Context;Ljava/lang/String;IZ)Landroid/widget/Button;

    move-result-object v3

    invoke-static {v0}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->cell(Landroid/content/Context;)Landroid/widget/LinearLayout$LayoutParams;

    move-result-object v4

    invoke-virtual {v2, v3, v4}, Landroid/widget/LinearLayout;->addView(Landroid/view/View;Landroid/view/ViewGroup$LayoutParams;)V

    const-string v3, "MAX"

    const/4 v4, 0x4

    const/4 v5, 0x1

    invoke-static {v0, v3, v4, v5}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->btn(Landroid/content/Context;Ljava/lang/String;IZ)Landroid/widget/Button;

    move-result-object v3

    invoke-static {v0}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->cell(Landroid/content/Context;)Landroid/widget/LinearLayout$LayoutParams;

    move-result-object v4

    invoke-virtual {v2, v3, v4}, Landroid/widget/LinearLayout;->addView(Landroid/view/View;Landroid/view/ViewGroup$LayoutParams;)V

    invoke-virtual {v1, v2}, Landroid/widget/LinearLayout;->addView(Landroid/view/View;)V

    const-string v2, "EXPERIENCE"

    invoke-static {v0, v2}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->hdr(Landroid/content/Context;Ljava/lang/String;)Landroid/widget/TextView;

    move-result-object v2

    invoke-virtual {v1, v2}, Landroid/widget/LinearLayout;->addView(Landroid/view/View;)V

    invoke-static {v0}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->row(Landroid/content/Context;)Landroid/widget/LinearLayout;

    move-result-object v2

    const-string v3, "+10K XP"

    const/4 v4, 0x5

    const/4 v5, 0x0

    invoke-static {v0, v3, v4, v5}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->btn(Landroid/content/Context;Ljava/lang/String;IZ)Landroid/widget/Button;

    move-result-object v3

    invoke-static {v0}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->cell(Landroid/content/Context;)Landroid/widget/LinearLayout$LayoutParams;

    move-result-object v4

    invoke-virtual {v2, v3, v4}, Landroid/widget/LinearLayout;->addView(Landroid/view/View;Landroid/view/ViewGroup$LayoutParams;)V

    const-string v3, "MAX XP"

    const/4 v4, 0x6

    const/4 v5, 0x1

    invoke-static {v0, v3, v4, v5}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->btn(Landroid/content/Context;Ljava/lang/String;IZ)Landroid/widget/Button;

    move-result-object v3

    invoke-static {v0}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->cell(Landroid/content/Context;)Landroid/widget/LinearLayout$LayoutParams;

    move-result-object v4

    invoke-virtual {v2, v3, v4}, Landroid/widget/LinearLayout;->addView(Landroid/view/View;Landroid/view/ViewGroup$LayoutParams;)V

    invoke-virtual {v1, v2}, Landroid/widget/LinearLayout;->addView(Landroid/view/View;)V


    const-string v2, "ZOMBIE PERKS"

    invoke-static {v0, v2}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->hdr(Landroid/content/Context;Ljava/lang/String;)Landroid/widget/TextView;

    move-result-object v2

    invoke-virtual {v1, v2}, Landroid/widget/LinearLayout;->addView(Landroid/view/View;)V

    invoke-static {v0}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->row(Landroid/content/Context;)Landroid/widget/LinearLayout;

    move-result-object v2

    const-string v3, "VIEW PERKS & ABILITIES"

    const/4 v4, 0x7

    const/4 v5, 0x1

    invoke-static {v0, v3, v4, v5}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->btn(Landroid/content/Context;Ljava/lang/String;IZ)Landroid/widget/Button;

    move-result-object v3

    invoke-static {v0}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->cell(Landroid/content/Context;)Landroid/widget/LinearLayout$LayoutParams;

    move-result-object v4

    invoke-virtual {v2, v3, v4}, Landroid/widget/LinearLayout;->addView(Landroid/view/View;Landroid/view/ViewGroup$LayoutParams;)V

    invoke-virtual {v1, v2}, Landroid/widget/LinearLayout;->addView(Landroid/view/View;)V

    const-string v2, "RAID BOSS"

    invoke-static {v0, v2}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->hdr(Landroid/content/Context;Ljava/lang/String;)Landroid/widget/TextView;

    move-result-object v2

    invoke-virtual {v1, v2}, Landroid/widget/LinearLayout;->addView(Landroid/view/View;)V

    invoke-static {v0}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->row(Landroid/content/Context;)Landroid/widget/LinearLayout;

    move-result-object v2

    const-string v3, "BOSS STATUS & REWARDS"

    const/16 v4, 0x8

    const/4 v5, 0x1

    invoke-static {v0, v3, v4, v5}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->btn(Landroid/content/Context;Ljava/lang/String;IZ)Landroid/widget/Button;

    move-result-object v3

    invoke-static {v0}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->cell(Landroid/content/Context;)Landroid/widget/LinearLayout$LayoutParams;

    move-result-object v4

    invoke-virtual {v2, v3, v4}, Landroid/widget/LinearLayout;->addView(Landroid/view/View;Landroid/view/ViewGroup$LayoutParams;)V

    invoke-virtual {v1, v2}, Landroid/widget/LinearLayout;->addView(Landroid/view/View;)V
    invoke-static {v0}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->div(Landroid/content/Context;)Landroid/view/View;

    move-result-object v2

    new-instance v3, Landroid/widget/LinearLayout$LayoutParams;

    const/4 v4, -0x1

    const/4 v5, 0x2

    invoke-static {v0, v5}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->dp(Landroid/content/Context;I)I

    move-result v5

    invoke-direct {v3, v4, v5}, Landroid/widget/LinearLayout$LayoutParams;-><init>(II)V

    const/4 v4, 0x0

    const/16 v5, 0x8

    invoke-static {v0, v5}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->dp(Landroid/content/Context;I)I

    move-result v5

    invoke-virtual {v3, v4, v5, v4, v5}, Landroid/widget/LinearLayout$LayoutParams;->setMargins(IIII)V

    invoke-virtual {v1, v2, v3}, Landroid/widget/LinearLayout;->addView(Landroid/view/View;Landroid/view/ViewGroup$LayoutParams;)V

    new-instance v2, Landroid/widget/ScrollView;

    invoke-direct {v2, v0}, Landroid/widget/ScrollView;-><init>(Landroid/content/Context;)V

    const/4 v3, 0x0

    invoke-virtual {v2, v3}, Landroid/widget/ScrollView;->setVerticalScrollBarEnabled(Z)V

    invoke-virtual {v2, v1}, Landroid/widget/ScrollView;->addView(Landroid/view/View;)V

    new-instance v3, Landroid/app/AlertDialog$Builder;

    invoke-direct {v3, v0}, Landroid/app/AlertDialog$Builder;-><init>(Landroid/content/Context;)V

    invoke-virtual {v3, v2}, Landroid/app/AlertDialog$Builder;->setView(Landroid/view/View;)Landroid/app/AlertDialog$Builder;

    const-string v4, "CLOSE"

    const/4 v5, 0x0

    invoke-virtual {v3, v4, v5}, Landroid/app/AlertDialog$Builder;->setNegativeButton(Ljava/lang/CharSequence;Landroid/content/DialogInterface$OnClickListener;)Landroid/app/AlertDialog$Builder;

    invoke-virtual {v3}, Landroid/app/AlertDialog$Builder;->create()Landroid/app/AlertDialog;

    move-result-object v8

    invoke-virtual {v8}, Landroid/app/AlertDialog;->show()V

    const/4 v2, 0x1

    invoke-virtual {v8, v2}, Landroid/app/AlertDialog;->setCanceledOnTouchOutside(Z)V

    const/4 v2, -0x2

    invoke-virtual {v8, v2}, Landroid/app/AlertDialog;->getButton(I)Landroid/widget/Button;

    move-result-object v2

    if-eqz v2, :cond_win

    const/4 v3, 0x0

    invoke-virtual {v2, v3}, Landroid/widget/Button;->setAllCaps(Z)V

    const/16 v3, 0xe

    int-to-float v3, v3

    invoke-virtual {v2, v3}, Landroid/widget/Button;->setTextSize(F)V

    const-string v3, "#E06A5A"

    invoke-static {v3}, Landroid/graphics/Color;->parseColor(Ljava/lang/String;)I

    move-result v3

    invoke-virtual {v2, v3}, Landroid/widget/Button;->setTextColor(I)V

    sget-object v3, Landroid/graphics/Typeface;->DEFAULT_BOLD:Landroid/graphics/Typeface;

    invoke-virtual {v2, v3}, Landroid/widget/Button;->setTypeface(Landroid/graphics/Typeface;)V

    const/4 v3, 0x0

    invoke-static {v0, v3}, Lcom/capcom/zombiecafeandroid/DebugMenuButtonListener;->btnBg(Landroid/content/Context;Z)Landroid/graphics/drawable/StateListDrawable;

    move-result-object v3

    invoke-virtual {v2, v3}, Landroid/widget/Button;->setBackground(Landroid/graphics/drawable/Drawable;)V

    invoke-virtual {v2}, Landroid/widget/Button;->getLayoutParams()Landroid/view/ViewGroup$LayoutParams;

    move-result-object v3

    if-eqz v3, :cond_win

    const/4 v4, -0x1

    iput v4, v3, Landroid/view/ViewGroup$LayoutParams;->width:I

    invoke-virtual {v2, v3}, Landroid/widget/Button;->setLayoutParams(Landroid/view/ViewGroup$LayoutParams;)V

    :cond_win
    invoke-virtual {v8}, Landroid/app/AlertDialog;->getWindow()Landroid/view/Window;

    move-result-object v2

    if-eqz v2, :cond_end


    const v3, 0x1000000

    invoke-virtual {v2, v3}, Landroid/view/Window;->clearFlags(I)V
    new-instance v3, Landroid/graphics/drawable/ColorDrawable;

    const/4 v4, 0x0

    invoke-direct {v3, v4}, Landroid/graphics/drawable/ColorDrawable;-><init>(I)V

    invoke-virtual {v2, v3}, Landroid/view/Window;->setBackgroundDrawable(Landroid/graphics/drawable/Drawable;)V

    invoke-virtual {v0}, Landroid/content/Context;->getResources()Landroid/content/res/Resources;

    move-result-object v3

    invoke-virtual {v3}, Landroid/content/res/Resources;->getDisplayMetrics()Landroid/util/DisplayMetrics;

    move-result-object v3

    iget v3, v3, Landroid/util/DisplayMetrics;->widthPixels:I

    mul-int/lit8 v3, v3, 0x7

    div-int/lit8 v3, v3, 0x8

    const/4 v4, -0x2

    invoke-virtual {v2, v3, v4}, Landroid/view/Window;->setLayout(II)V

    :cond_end
    return-void
.end method
