// For conditions of distribution and use, see copyright notice in license.txt

#include "StableHeaders.h"
#include "Renderer.h"
#include "RendererEvents.h"
#include "ResourceHandler.h"
#include "OgreRenderingModule.h"

#include <Ogre.h>

using namespace Foundation;

namespace OgreRenderer
{
    //! Responds to Ogre's window events and manages the Realxtend world render window.
    class EventListener : public Ogre::WindowEventListener
    {
    public:
        EventListener(Renderer* renderer) :
            renderer_(renderer)
        {
        }

        ~EventListener() 
        {
        }
        
        void windowResized(Ogre::RenderWindow* rw)
        {
            if (rw == renderer_->renderwindow_)
            {
                if (renderer_->camera_)
                    renderer_->camera_->setAspectRatio(Ogre::Real(rw->getWidth() / Ogre::Real(rw->getHeight())));

                Events::WindowResized data(rw->getWidth(), rw->getHeight());
                renderer_->framework_->GetEventManager()->SendEvent(renderer_->renderercategory_id_, Events::WINDOW_RESIZED, &data);
            }
        }
    
        void windowClosed(Ogre::RenderWindow* rw)
        {
            if (rw == renderer_->renderwindow_)
            {
                renderer_->OnWindowClosed();
            }
        }
        
    private:
        Renderer* renderer_;
    };

/////////////////////////////////////////////////////////////

    //! Ogre log listener, for passing ogre log messages
    class LogListener : public Ogre::LogListener
    {
    public:
        LogListener() : Ogre::LogListener() {}
        virtual ~LogListener() {}

        //! Subscribe new listener for ogre log messages
        void SubscribeListener(const Foundation::LogListenerPtr &listener)
        {
            listeners_.push_back(listener);
        }

        //! Unsubscribe listener for ogre log messages
        void UnsubscribeListener(const Foundation::LogListenerPtr &listener)
        {
            listeners_.remove(listener);
        }

        void messageLogged( const std::string& message, Ogre::LogMessageLevel lml, bool maskDebug, const std::string &logName )
        {
            for (ListenerList::iterator listener = listeners_.begin() ;
                  listener != listeners_.end() ; 
                  ++listener)
            {
                (*listener)->LogMessage(message);
            }
        }
    private:
        typedef std::list<Foundation::LogListenerPtr> ListenerList;

        //! list of subscribed listeners
        ListenerList listeners_;
    };

/////////////////////////////////////////////////////////

    Renderer::Renderer(Framework* framework, const std::string& config, const std::string& plugins) :
        initialized_(false),
        framework_(framework),
        scenemanager_(NULL),
        camera_(NULL),
        renderwindow_(NULL),
        object_id_(0),
        main_window_handle_(0),
        listener_(EventListenerPtr(new EventListener(this))),
        log_listener_(OgreLogListenerPtr(new LogListener)),
        resource_handler_(ResourceHandlerPtr(new ResourceHandler(framework))),
        config_filename_ (config),
        plugins_filename_ (plugins)
    {
        Foundation::EventManagerPtr event_manager = framework_->GetEventManager();
        
        renderercategory_id_ = event_manager->RegisterEventCategory("Renderer");
        event_manager->RegisterEvent(renderercategory_id_, Events::POST_RENDER, "PostRender");
        event_manager->RegisterEvent(renderercategory_id_, Events::WINDOW_CLOSED, "WindowClosed");
        event_manager->RegisterEvent(renderercategory_id_, Events::WINDOW_RESIZED, "WindowResized");
    }
    
    Renderer::~Renderer()
    {
        if (initialized_)
        {
            Ogre::LogManager::getSingleton().getDefaultLog()->removeListener(log_listener_.get());
            Ogre::WindowEventUtilities::removeWindowEventListener(renderwindow_, listener_.get());
        }
        log_listener_.reset();

        resource_handler_.reset();

        root_.reset();
    }
    
    void Renderer::Initialize()
    {
        if (initialized_)
            return;
            
        std::string logfilepath = framework_->GetPlatform()->GetUserDocumentsDirectory();
        logfilepath += "/Ogre.log";

        root_ = OgreRootPtr(new Ogre::Root("", config_filename_, logfilepath));
        Ogre::LogManager::getSingleton().getDefaultLog()->addListener(log_listener_.get());
        LoadPlugins(plugins_filename_);
        
#ifdef _WINDOWS
        std::string rendersystem_name = framework_->GetDefaultConfig().DeclareSetting<std::string>("OgreRenderer", "RenderSystem", "Direct3D9 Rendering Subsystem");
#else
        std::string rendersystem_name = "OpenGL Rendering Subsystem";
        framework_->GetDefaultConfig().DeclareSetting("OgreRenderer", "RenderSystem", rendersystem_name);
#endif
        int width = framework_->GetDefaultConfig().DeclareSetting("OgreRenderer", "WindowWidth", 800);
        int height = framework_->GetDefaultConfig().DeclareSetting("OgreRenderer", "WindowHeight", 600);
        bool fullscreen = framework_->GetDefaultConfig().DeclareSetting("OgreRenderer", "Fullscreen", false);
        
        Ogre::RenderSystem* rendersystem = root_->getRenderSystemByName(rendersystem_name);
        
        if (!rendersystem)
        {
            throw Core::Exception("Could not find Ogre rendersystem.");
        }

        // GTK's pango/cairo/whatever's font rendering doesn't work if the floating point mode is not set to strict.
        // This however creates undefined behavior for D3D (refrast + D3DX), but we aren't using those anyway.
        Ogre::ConfigOptionMap& map = rendersystem->getConfigOptions();
        if (map.find("Floating-point mode") != map.end())
            rendersystem->setConfigOption("Floating-point mode", "Consistent");
        
        root_->setRenderSystem(rendersystem);
        root_->initialise(false);

        Ogre::NameValuePairList params;
        std::string application_name = framework_->GetDefaultConfig().GetSetting<std::string>(Foundation::Framework::ConfigurationGroup(), "application_name");

        if (!external_window_parameter_.empty()) 
            params["externalWindowHandle"] = external_window_parameter_;

        try
        {
            renderwindow_ = root_->createRenderWindow(application_name, width, height, fullscreen, &params);
        }
        catch (Ogre::Exception e) {}
        
        if (!renderwindow_)
        {
            throw Core::Exception("Could not create Ogre rendering window");
        }

        SetupResources();
        SetupScene();
        
        Ogre::WindowEventUtilities::addWindowEventListener(renderwindow_, listener_.get());
        
        initialized_ = true;
    }

    void Renderer::PostInitialize()
    {
        resource_handler_->PostInitialize();
    }

    void Renderer::LoadPlugins(const std::string& plugin_filename)
    {
        Ogre::ConfigFile file;
        try
        {
            file.load(plugin_filename);
        }
        catch (Ogre::Exception e)
        {
            OgreRenderingModule::LogError("Could not load Ogre plugins configuration file");
            return;
        }

        Ogre::String plugin_dir = file.getSetting("PluginFolder");
        Ogre::StringVector plugins = file.getMultiSetting("Plugin");
        
        if (plugin_dir.length())
        {
            if ((plugin_dir[plugin_dir.length() - 1] != '\\') && (plugin_dir[plugin_dir.length() - 1] != '/'))
            {
#ifdef _WINDOWS
                plugin_dir += "\\";
#else
                plugin_dir += "/";
#endif
            }
        }
        
        for (Core::uint i = 0; i < plugins.size(); ++i)
        {
            try
            {
                root_->loadPlugin(plugin_dir + plugins[i]);
            }
            catch (Ogre::Exception e)
            {
                OgreRenderingModule::LogError("Plugin " + plugins[i] + " failed to load");
            }
        }
    }
    
    void Renderer::SetupResources()
    {
        Ogre::ConfigFile cf;
        cf.load("resources.cfg");

        Ogre::ConfigFile::SectionIterator seci = cf.getSectionIterator();
        Ogre::String sec_name, type_name, arch_name;
        
        while(seci.hasMoreElements())
        {
            sec_name = seci.peekNextKey();
            Ogre::ConfigFile::SettingsMultiMap* settings = seci.getNext();
            Ogre::ConfigFile::SettingsMultiMap::iterator i;
            for(i = settings->begin(); i != settings->end(); ++i)
            {
                type_name = i->first;
                arch_name = i->second;
                Ogre::ResourceGroupManager::getSingleton().addResourceLocation(arch_name, type_name, sec_name);
            }
        }
        
        Ogre::ResourceGroupManager::getSingleton().initialiseAllResourceGroups();
    }
    
    void Renderer::SetupScene()
    {
        scenemanager_ = root_->createSceneManager(Ogre::ST_GENERIC, "SceneManager");
        camera_ = scenemanager_->createCamera("Camera");
        camera_->setNearClipDistance(0.1f);
        camera_->setFarClipDistance(2000.f);
        Ogre::Viewport* viewport = renderwindow_->addViewport(camera_);
        camera_->setAspectRatio(Ogre::Real(viewport->getActualWidth()) / Ogre::Real(viewport->getActualHeight()));
    }

    size_t Renderer::GetWindowHandle() const
    {
        size_t window_handle = 0;

        if (external_window_parameter_.empty())
        {
            if (renderwindow_)
                renderwindow_->getCustomAttribute("WINDOW", &window_handle);
        } else
        {
            window_handle = main_window_handle_;
        }

        return window_handle;
    }
    int Renderer::GetWindowWidth() const
    {
        int size = 0;
        if (renderwindow_)
            size = renderwindow_->getWidth();
        return size;
    }
    int Renderer::GetWindowHeight() const
    {
        int size = 0;
        if (renderwindow_)
            size = renderwindow_->getHeight();
        return size;
    }

    void Renderer::SubscribeLogListener(const Foundation::LogListenerPtr &listener)
    {
        log_listener_->SubscribeListener(listener);
    }

    void Renderer::UnsubscribeLogListener(const Foundation::LogListenerPtr &listener)
    {
        log_listener_->UnsubscribeListener(listener);
    }
    
    void Renderer::Update(Core::f64 frametime)
    {
        Ogre::WindowEventUtilities::messagePump();
    }
    
    void Renderer::Resize(Core::uint width, Core::uint height)
    {
        if (renderwindow_)
        {
            renderwindow_->resize(width, height);
        }
    }

    void Renderer::OnWindowClosed()
    {
        framework_->Exit();
        framework_->GetEventManager()->SendEvent(renderercategory_id_, Events::WINDOW_CLOSED, NULL);
    }

    void Renderer::Render()
    {
        if (!initialized_) return;
        
        root_->_fireFrameStarted();
        
        // Render without swapping buffers first
        Ogre::RenderSystem* renderer = root_->getRenderSystem();
        renderer->_updateAllRenderTargets(false);
        // Send postrender event, so that custom rendering may be added
        framework_->GetEventManager()->SendEvent(renderercategory_id_, Events::POST_RENDER, NULL);
        // Swap buffers now
        renderer->_swapAllRenderTargetBuffers(renderer->getWaitForVerticalBlank());

        root_->_fireFrameEnded();
    }
    
    std::string Renderer::GetUniqueObjectName()
    {
        return "obj" + Core::ToString<Core::uint>(object_id_++);
    }

    ResourcePtr Renderer::GetResource(const std::string& id, const std::string& type)
    {
        return resource_handler_->GetResource(id, type);
    }
    
    Core::request_tag_t Renderer::RequestResource(const std::string& id, const std::string& type)   
    {
        return resource_handler_->RequestResource(id, type);
    }
    
    void Renderer::RemoveResource(const std::string& id, const std::string& type)
    {
        return resource_handler_->RemoveResource(id, type);    
    }
}

