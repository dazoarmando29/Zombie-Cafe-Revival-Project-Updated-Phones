/* Seasonal weather overlay: screen-space particles + theme tint drawn
 * over EVERY engine frame (menu, cafe, raids) via real GLES1 calls from
 * the host's render wrapper. No engine state is touched; matrices and
 * client state are restored before returning.
 */
#pragma once

namespace zcr_weather {

/* event_id: 1=halloween, 2=christmas, 3=easter, 0=off (draws nothing). */
void on_frame(int event_id, float delta_s);

}  // namespace zcr_weather
