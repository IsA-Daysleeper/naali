// For conditions of distribution and use, see copyright notice in LICENSE

#pragma once

#include "FrameworkFwd.h"

#include <QObject>
#include <QStringList>

#include <boost/smart_ptr.hpp>

#include <vector>

/// The system root access object.
class Framework : public QObject
{
    Q_OBJECT

public:
    /// Constructs and initializes the framework.
    /** @param argc Command line argument count as provided by the operating system.
        @param argv Command line arguments as provided by the operating system. */
    Framework(int argc, char** argv);

    ~Framework();

    /// Entry point for the framework.
    void Go();

    /// Runs through a single frame of logic update and rendering.
    void ProcessOneFrame();

    /// Returns module by class T.
    /** @param T class type of the module.
        @return The module, or null if the module doesn't exist. Always remember to check for null pointer.
        @note Do not store the returned raw module pointer anywhere or make a boost::weak_ptr/shared_ptr out of it. */
    template <class T>
    T *GetModule() const;

    /// Registers a new module into the Framework.
    /** Framework will take ownership of the module pointer, so it is safe to pass in a raw pointer. */
    void RegisterModule(IModule *module);

    /// Stores the given QObject as a dynamic property into the Framework.
    /** This is done to implement easier script access for QObject-based interface objects.
        @param name The name to use for the property. Fails if name is an empty string.
        @param object The object to register as a property. The property will be a QVariant containing this QObject.
        @return If the registration succeeds, this returns true. Otherwise either 'object' pointer was null,
               or a property with that name was registered already.
        @note There is no unregister option. It can be implemented if someone finds it useful, but at this point
         we are going with a "unload-only-on-close" behavior. */
    bool RegisterDynamicObject(QString name, QObject *object);

    /// Cancel a pending exit
    void CancelExit();

    /// Force immediate exit, with no possibility to cancel it
    void ForceExit();

    /// Returns true if framework is in the process of exiting (will exit at next possible opportunity)
    bool IsExiting() const { return exitSignal; }

#ifdef PROFILING
    /// Returns the default profiler used by all normal profiling blocks. For profiling code, use PROFILE-macro.
    Profiler *GetProfiler() const;
#endif
    /// Returns the main QApplication
    Application *App() const;

public slots:
    /// Returns the core API UI object.
    /** @note Never returns a null pointer. Use IsHeadless() to check out if we're running the headless mode or not. */
    UiAPI *Ui() const;

    /// Returns the core API Input object.
    InputAPI *Input() const;

    /// Returns the core API Frame object.
    FrameAPI *Frame() const;

    /// Returns the core API Console object.
    ConsoleAPI *Console() const;

    /// Returns the core API Audio object.
    AudioAPI *Audio() const;

    /// Returns core API Asset object.
    AssetAPI *Asset() const;

    /// Returns core API Scene object.
    SceneAPI *Scene() const;

    /// Returns core API Config object.
    ConfigAPI *Config() const;

    /// Returns core API Plugin object.
    PluginAPI *Plugins() const;

    /// Returns Tundra API version info object.
    ///\todo Delete/simplify.
    ApiVersionInfo *ApiVersion() const;

    /// Returns Tundra application version info object.
    ///\todo Delete/simplify.
    ApplicationVersionInfo *ApplicationVersion() const;

    /// Registers the system Renderer object.
    /** @note Please don't use this function. Called only by the OgreRenderingModule which implements the rendering subsystem. */
    void RegisterRenderer(IRenderer *renderer);

    /// Returns the system Renderer object.
    /** @note Please don't use this function. It exists for dependency inversion purposes only.
        Instead, call framework->GetModule<OgreRenderer::OgreRenderingModule>()->Renderer(); to directly obtain the renderer,
        as that will make the dependency explicit. The IRenderer interface is not continuously updated to match the real Renderer implementation. */
    IRenderer *Renderer() const;

    /// Stores the Framework instance. Call this inside each plugin DLL main function that will have a copy of the static instance pointer.
    static void SetInstance(Framework *fw) { instance = fw; }

    /// Returns the global Framework instance.
    /** @note DO NOT CALL THIS FUNCTION. Every point where this function is called will cause a serious portability issue when we intend
        to run multiple instances inside a single process (inside a browser memory space). This function is intended to serve only for 
        carefully crafted re-entrant code (currently only logging and profiling). */
    static Framework *Instance() { return instance; }

    /// Returns raw module pointer.
    /** @param name Name of the module.
        @note Do not store the returned raw module pointer anywhere or make a boost::weak_ptr/shared_ptr out of it. */
    IModule *GetModuleByName(const QString &name) const;

    /// Returns if we're running the application in headless or not.
    bool IsHeadless() const { return headless; }

    /// Signals the framework to exit
    void Exit();

    /// Returns whether or not the command line arguments contain a specific value.
    /** @param value Key or value with possible prefixes, case-insensitive. */
    bool HasCommandLineParameter(const QString &value) const;

    /// Returns list of command line parameter values for a specific @c key, f.ex. "--file".
    /** Value is considered to be the command line argument following the @c key.
        If the argument following @c key is another key-type argument (--something), it's not appended to the return list.
        @param key Key with possible prefixes, case-insensitive */
    QStringList CommandLineParameters(const QString &key) const;

    /// Prints to console all the used startup options.
    void PrintStartupOptions();

private:
    Q_DISABLE_COPY(Framework)

    /// Appends all found startup options from the given file to the startupOptions member.
    void LoadStartupOptionsFromXML(QString configurationFile);

    bool exitSignal; ///< If true, exit application.
#ifdef PROFILING
    Profiler *profiler; ///< Profiler.
#endif
    ProfilerQObj *profilerQObj; ///< We keep this QObject always alive, even when profiling is not enabled, so that scripts don't have to check whether profiling is enabled or disabled.
    bool headless; ///< Are we running in the headless mode.
    Application *application; ///< The main QApplication object.
    FrameAPI *frame; ///< The Frame API.
    ConsoleAPI *console; ///< The console API.
    UiAPI *ui; ///< The UI API.
    InputAPI *input; ///< The Input API.
    AssetAPI *asset; ///< The Asset API.
    AudioAPI *audio; ///< The Audio API.
    SceneAPI *scene; ///< The Scene API.
    ConfigAPI *config; ///< The Config API.
    PluginAPI *plugin;
    IRenderer *renderer;

    /// Stores all command line parameters and startup options specified in the Config XML files.
    QStringList startupOptions;

    /// The Tundra API version info of this build. May differ from the end user 
    /// application version of the default distribution, i.e. app may change when api stays same.
    ///\todo Delete/simplify.
    ApiVersionInfo *apiVersionInfo;

    /// The Tundra application version info for this build.
    ///\todo Delete/simplify.
    ApplicationVersionInfo *applicationVersionInfo;

    /// Framework owns the memory of all the modules in the system. These are freed when Framework is exiting.
    std::vector<boost::shared_ptr<IModule> > modules;

    static Framework *instance;
    int argc; ///< Command line argument count as supplied by the operating system.
    char **argv; ///< Command line arguments as supplied by the operating system.
};

template <class T>
T *Framework::GetModule() const
{
    for(size_t i = 0; i < modules.size(); ++i)
    {
        T *module = dynamic_cast<T*>(modules[i].get());
        if (module)
            return module;
    }

    return 0;
}
