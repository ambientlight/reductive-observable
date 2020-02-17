type observableAction('action) =
  | Start('action, Rx.ReplaySubject.t('action))
  | Update('action, Rx.ReplaySubject.t('action))
  | Error('action, Rx.ReplaySubject.t('action))
  | End('action, Rx.ReplaySubject.t('action));

type t('action, 'state) = Reductive.Store.t(
  observableAction('action),
  Reductive.Store.t('action, 'state)
);

let observe = (store: t('action, 'state), action: 'action) => {
  let subject = Rx.ReplaySubject.create(());
  store |. Reductive.Store.dispatch(Start(action, subject));
  subject
  |> Rx.ReplaySubject.asObservable
}

let create = (store, ~enhancer=?, ()): t('action, 'state) => {
  Reductive.Store.create(
    ~reducer=(store, action) => {
      switch(action){
      | Start(action, _subject) => {
        Reductive.Store.dispatch(store, action)
      }
      | Update(action, subject) => {
        Reductive.Store.dispatch(store, action);
        subject |> Rx.ReplaySubject.next(action);
      }
      | Error(action, subject) => {
        Reductive.Store.dispatch(store, action);
        subject |> Rx.ReplaySubject.error(action);
      }
      | End(action, subject) => {
        Reductive.Store.dispatch(store, action);
        subject |> Rx.ReplaySubject.next(action);
        subject |> Rx.ReplaySubject.complete;
      }}

      store
    }, 
    ~preloadedState=store, 
    ~enhancer?,
    ()
  )
};

let unsubscribe = (store: t('action, 'state), listener, ()) => 
  store
  |. Reductive.Store.getState
  |. Reductive.Store.unsubscribe(listener, ());

let subscribe = (store: t('action, 'state), listener, ()) => 
  store
  |. Reductive.Store.getState
  |. Reductive.Store.subscribe(listener, ());

let nativeDispatch = (store: t('action, 'state), action) => 
  store
  |. Reductive.Store.getState
  |. Reductive.Store.nativeDispatch(action);

let dispatch = (store: t('action, 'state), action) =>
  store
  |. observe(action)
  |> ignore;

let getState = (store: t('action, 'state)) => 
  store 
  |. Reductive.Store.getState
  |. Reductive.Store.getState;

let replaceReducer = (store: t('action, 'state), reducer) => 
  store
  |. Reductive.Store.getState
  |. Reductive.Store.replaceReducer(reducer);