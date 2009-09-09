// For conditions of distribution and use, see copyright notice in license.txt

#ifndef incl_Foundation_ThreadTaskManager_h
#define incl_Foundation_ThreadTaskManager_h

#include "ThreadTask.h"

namespace Foundation
{
    class Framework;
    
    //! Takes ownership of ThreadTasks to handle results from them. Necessary to use ThreadTasks in queued result mode.
    /*! There exists a system-wide ThreadTaskManager in the framework, but nothing prevents you creating your own additional ThreadTaskManagers.
     */
    class ThreadTaskManager
    {
        friend class ThreadTask;
        
    public:
        //! Constructor
        /*! \param framework Framework, needed for sending events
         */
        ThreadTaskManager(Framework* framework);
        
        //! Destructor
        ~ThreadTaskManager();
        
        //! Adds a ThreadTask
        /*! \param task Task to add
            To not lose any queued results, adding the task to the manager should always be done before adding work requests to the task.
         */
        void AddThreadTask(ThreadTaskPtr task);
        
        //! Removes a ThreadTask
        /*! \param task Task to remove
         */
        void RemoveThreadTask(ThreadTaskPtr task);
        
        //! Adds a request by task description
        /*! \param task_description Task description
            \param request Task request
            \return true if a matching ThreadTask was found to perform request, false if not
            Note: currently simply the first matching ThreadTask will be used; there is no load balancing
         */
        bool AddRequest(const std::string& task_description, ThreadTaskRequestPtr request);
        
        template <class T> void AddRequest(const std::string& task_description, boost::shared_ptr<T> request)
        {
            AddRequest(task_description, boost::dynamic_pointer_cast<ThreadTaskRequest>(request));
        }
        
        //! Checks for results and sends them as events. Deletes finished ThreadTasks.
        void SendResultEvents();
        
        //! Gets all results. Does not send them as events. Deletes finished ThreadTasks.
        std::vector<ThreadTaskResultPtr> GetResults();
        
        //! Gets results matching a certain task description. Does not send them as events. Deletes finished ThreadTasks matching description.
        std::vector<ThreadTaskResultPtr> GetResults(const std::string& task_description);
        
    private:
        //! Queues a result. Called from ThreadTask work thread.
        void QueueResult(ThreadTaskResultPtr result);
        
        //! Owned ThreadTasks
        std::vector<ThreadTaskPtr> tasks_;
        
        //! Result queue
        std::list<ThreadTaskResultPtr> results_;
        
        //! Result queue mutex
        Core::Mutex result_mutex_;
        
        //! Framework
        Framework* framework_;
    };
}

#endif // incl_Foundation_ThreadTaskManager_h