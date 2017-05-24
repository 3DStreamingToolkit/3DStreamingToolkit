#pragma once


#include "IUnityInterface.h"
#include <stdint.h>

/*
    Plugin callback events
    ======================

    These events will be propagated to all plugins that implement void UnityRenderingExtEvent(GfxDeviceRenderingExtEventType event, void* data);
    In order to not have conflicts of IDs the plugin should query IUnityRenderingExtConfig::ReserveEventIDRange and use the returned index as
    an offset for the built-in plugin event enums. It should also export it to scripts to be able to offset the script events too.


    // Native code example

    enum PluginCustomCommands
    {
        kPluginCustomCommandDownscale,
        kPluginCustomCommandUpscale,

        // insert your own events here

        kPluginCustomCommandCount
    };

    static IUnityInterfaces* s_UnityInterfaces = NULL;
    static  = NULL;
    int s_MyPluginEventStartIndex;

    extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API
    UnityPluginLoad(IUnityInterfaces* unityInterfaces)
    {
        // Called before DX device is created
        s_UnityInterfaces = unityInterfaces;
        IUnityRenderingExtConfig* unityRenderingExtConfig = s_UnityInterfaces->Get<IUnityRenderingExtConfig>();
        s_MyPluginEventStartIndex = unityRenderingExtConfig->ReserveEventIDRange(kPluginCustomCommandCount);

        // More initialization code here...

    }

    static GfxDeviceRenderingExtEventType RemapToCustomEvent(GfxDeviceRenderingExtEventType event)
    {
        return (GfxDeviceRenderingExtEventType)((int)event - s_MyPluginEventStartIndex);
    }

    extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API
    UnityRenderingExtEvent(GfxDeviceRenderingExtEventType event, void* data)
    {
        switch(event)
        {
            case kUnityRenderingExtEventSetStereoTarget:
            case kUnityRenderingExtEventBeforeDrawCall:
            case kUnityRenderingExtEventAfterDrawCall:
            case kUnityRenderingExtEventCustomGrab:
            case kUnityRenderingExtEventCustomBlit:
                ProcessBuiltinEvent(event, data);
                break;
            default:
                ProcessCustomEvent(RemapToCustomEvent(event) , data);
        }
    }

*/
enum GfxDeviceRenderingExtEventType
{
    kUnityRenderingExtEventSetStereoTarget,                 // issued during SetStereoTarget and carrying the current 'eye' as parameter
    kUnityRenderingExtEventBeforeDrawCall,                  // issued during BeforeDrawCall and carrying UnityRenderingExtBeforeDrawCallParams as parameter
    kUnityRenderingExtEventAfterDrawCall,                   // issued during AfterDrawCall. This event doesn't carry any parameters
    kUnityRenderingExtEventCustomGrab,                      // issued during GrabIntoRenderTexture since we can't simply copy the resources
                                                            //      when custom rendering is used - we need to let plugin handle this. It carries over
                                                            //      a UnityRenderingExtCustomBlitParams params = { X, source, dest, 0, 0 } ( X means it's irrelevant )
    kUnityRenderingExtEventCustomBlit,                      // issued by plugin to insert custom blits. It carries over UnityRenderingExtCustomBlitParams as param.

    // keep this last
    kUnityRenderingExtEventCount,
    kUnityRenderingExtUserEventsStart = kUnityRenderingExtEventCount
};


/*
    This will be propagated to all plugins implementing UnityRenderingExtQuery.
*/
enum GfxDeviceRenderingExtQueryType
{
    kUnityRenderingExtQueryOverrideViewport         = 1 << 0,
    kUnityRenderingExtQueryOverrideScissor          = 1 << 1,
    kUnityRenderingExtQueryOverrideVROcclussionMesh = 1 << 2,
    kUnityRenderingExtQueryOverrideVRSinglePass     = 1 << 3,
};


struct UnityRenderingExtBeforeDrawCallParams
{
    void*   vertexShader;
    void*   fragmentShader;
    void*   geometryShader;
    void*   hullShader;
    void*   domainShader;
    int     eyeIndex;
};


struct UnityRenderingExtCustomBlitParams
{
    UnityTextureID source;
    UnityRenderBuffer destination;
    unsigned int command;
    unsigned int commandParam;
    unsigned int commandFlags;
};


// Interface to help setup / configure both unity and the plugin.


UNITY_DECLARE_INTERFACE(IUnityRenderingExtConfig)
{
public:
    int(UNITY_INTERFACE_API * ReserveEventIDRange)(int count);  // reserves 'count' event IDs. Plugins should use the result as a base index when issuing events back and forth to avoid event id clashes.
};
UNITY_REGISTER_INTERFACE_GUID(0xC31EF5E2832040ECULL, 0x908851F4B4B685CBULL, IUnityRenderingExtConfig)


// Certain Unity APIs (GL.IssuePluginEventAndData, CommandBuffer.IssuePluginEventAndData) can callback into native plugins.
// Provide them with an address to a function of this signature.
typedef void (UNITY_INTERFACE_API * UnityRenderingEventAndData)(int eventId, void* data);


#ifdef __cplusplus
extern "C" {
#endif

// If exported by a plugin, this function will be called for all the events in GfxDeviceRenderingExtEventType
void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityRenderingExtEvent(GfxDeviceRenderingExtEventType event, void* data);
// If exported by a plugin, this function will be called to query the plugin for the queries in GfxDeviceRenderingExtQueryType
bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityRenderingExtQuery(GfxDeviceRenderingExtQueryType query);

#ifdef __cplusplus
}
#endif
