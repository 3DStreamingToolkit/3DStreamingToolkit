using System;
using System.Collections;
using System.Collections.Generic;
using UnityEngine;

namespace Microsoft.Toolkit.ThreeD
{
    /// <summary>
    /// Simple singleton that provides threadsafe dispatching to the ui thread (main unity thread)
    /// </summary>
    public class UIThreadSingleton : MonoBehaviour
    {
        /// <summary>
        /// A string representation of the class name, since nameof() isn't available
        /// in unity mono just yet
        /// </summary>
        private static readonly string ClassName = "UIThreadSingleton";

        private static UIThreadSingleton internalInstance;
        private static UIThreadSingleton Instance
        {
            get
            {
                if (internalInstance == null)
                {
                    // note: this will only work from the main thread, so if you're only access of this class
                    // is from non standard threads, you should add this component to the scene manually
                    internalInstance = new GameObject(ClassName).AddComponent<UIThreadSingleton>();
                }

                return internalInstance;
            }
        }

        public static void Dispatch(Func<IEnumerator> work, ExecutionTime execTime = ExecutionTime.LateUpdate)
        {
            Instance.DispatchInternal(work, execTime);
        }

        public static void Dispatch(Action work, ExecutionTime execTime = ExecutionTime.LateUpdate)
        {
            Instance.DispatchInternal(() => { work(); return null; }, execTime);
        }

        public enum ExecutionTime
        {
            Update,
            LateUpdate,
            CoRoutine,
            new1
        }

#if UNITY_WSA
        private ConcurrentQueue<Func<IEnumerator>> updateQueue = new ConcurrentQueue<Func<IEnumerator>>();
        private ConcurrentQueue<Func<IEnumerator>> lateUpdateQueue = new ConcurrentQueue<Func<IEnumerator>>();
        private ConcurrentQueue<Func<IEnumerator>> coRoutineQueue = new ConcurrentQueue<Func<IEnumerator>>();
#else
        private Queue<Func<IEnumerator>> updateQueue = new Queue<Func<IEnumerator>>();
        private Queue<Func<IEnumerator>> lateUpdateQueue = new Queue<Func<IEnumerator>>();
        private Queue<Func<IEnumerator>> coRoutineQueue = new Queue<Func<IEnumerator>>();
#endif

        private void DispatchInternal(Func<IEnumerator> work, ExecutionTime execTime)
        {
            switch (execTime)
            {
                case ExecutionTime.Update:
                    updateQueue.Enqueue(work);
                    break;
                case ExecutionTime.LateUpdate:
                    lateUpdateQueue.Enqueue(work);
                    break;
                case ExecutionTime.CoRoutine:
                    coRoutineQueue.Enqueue(work);
                    break;
                default:
                    throw new InvalidOperationException("no such ExecutionTime");
            }
        }

        private void Awake()
        {
            if (internalInstance == null)
            {
                internalInstance = this;
            }
            else
            {
                Destroy(this.gameObject);
            }
        }

        private void Update()
        {
            Func<IEnumerator> action;
#if UNITY_WSA
            while (updateQueue.TryDequeue(out action))
            {
#else
            while (updateQueue.Count > 0)
            {
                lock (updateQueue)
                {
                    action = updateQueue.Dequeue();
                }
#endif
                action();
            }

            Func<IEnumerator> co;
#if UNITY_WSA
            while (coRoutineQueue.TryDequeue(out co))
            {
#else
            while (coRoutineQueue.Count > 0)
            {
                lock (coRoutineQueue)
                {
                    co = coRoutineQueue.Dequeue();
                }
#endif
                StartCoroutine(co());
            }
        }

        private void LateUpdate()
        {
            Func<IEnumerator> action;
#if UNITY_WSA
            while (lateUpdateQueue.TryDequeue(out action))
            {
#else
            while (lateUpdateQueue.Count > 0)
            {
                lock (lateUpdateQueue)
                {
                    action = lateUpdateQueue.Dequeue();
                }
#endif
                action();
            }
        }

        private void OnDestroy()
        {
            if (!(internalInstance == (this)))
            {
                return;
            }

            internalInstance = null;
        }
    }
}
