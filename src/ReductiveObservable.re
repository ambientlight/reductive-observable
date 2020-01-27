open Rx;
open Rx.Operators;

let middleware = (rootEpicObservable) => {
  /* observables passed to our epics */
  let actionStateSubject = Subject.create();
  
  /**
   * reductive doesn't partially apply store to middleware on init 
   * on each action it gets fully applied like middleware(store, next, action)
   * so we need keep subscriptionRef to know if we already fed epic subscription to the store dispatch 
   */
  let subscriptionRef: ref(option(Subscription.t)) = ref(None);

  /**
   * A BIT TRICKY
   * epic is passed inside observable and gets fully applied inside switchMap,
   * turning epic function observable into our action observable and essentially canceling out the previous epic
   * (call it 'replaceEpics' if your want)
   * 
   * this allows us to support hot reloading of epics
   */
  let epicInstantiator: Rx.Observable.t(('action, 'state)) => Rx.Observable.t('action) = (actionStateObservable) => 
    rootEpicObservable 
    |> switchMap((epic, _idx) => epic(actionStateObservable))
    |> observeOn(Rx.queue);

  (store: Reductive.Store.t('action, 'state), next: 'action => unit, action: 'action) => {
    if(subscriptionRef^ == None){
      let subscription = 
        epicInstantiator(actionStateSubject |. Subject.asObservable)
        |> Observable.subscribe(~next=Reductive.Store.dispatch(store));
      subscriptionRef := Some(subscription);
    };

    /* state update before epics recieve an action */
    next(action);
    actionStateSubject |> Subject.next((action, Reductive.Store.getState(store)));
  }
};

module Utils {
  /**
    Use this for side effects that do not emit actions
    By default, an observable side effect that you write is expected to emit an action back to the store.
   */
  let empty = observable => 
    observable 
    |> mergeMap(`Observable((_action, _idx) => Rx.empty));

  /** map and ignore actions where predicates returns None */
  let optmap = (project, observable) => 
    observable
    |> map((x, _idx) => project(x))
    |> filter((entry, _idx) => Belt.Option.isSome(entry))
    |> map((entry, _idx) => Belt.Option.getExn(entry));

};