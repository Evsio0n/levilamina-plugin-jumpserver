#include <ll/api/plugin/NativePlugin.h>

#include "Plugin.h"
extern void plugin_init();
extern void registerCommand(ll::plugin::NativePlugin& self);
namespace JumpServerPlugin {

Plugin::Plugin(ll::plugin::NativePlugin& self) : mSelf(self) {
    mSelf.getLogger().info("loading...");
    logger = &mSelf.getLogger();
    // Code for loading the plugin goes here.
}

bool Plugin::enable() {
    mSelf.getLogger().info("enabling...");
    registerCommand(mSelf);
    plugin_init();
    // Code for enabling the plugin goes here.

    return true;
}

bool Plugin::disable() {
    mSelf.getLogger().info("disabling...");

    // Code for disabling the plugin goes here.

    return true;
}

} // namespace plugin
