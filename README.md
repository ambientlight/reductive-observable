# reductive-observable
Minimal port of [redux-observable](https://github.com/redux-observable/redux-observable) to reason reductive.
Centalized rx side-effects for reductive.

Additionally this repo provides a higher order store `ObservableStore` that allows observing action-chains to bring a concept of `completion` to reductive.  To have completion in reductive, we would likely need to store the status of the side effect in a substate and then subscribe and observe changes to this substate, sometimes it might feel like an overkill, and we wished `dispatch()` would be able to return a `Promise` or `Observable` back, this is precisely what `ObservableStore.observe()` does, see [usage](https://github.com/ambientlight/reductive-observable#usage-observablestore).

## Installation

In addition to installing `reductive-observable` make sure you have `@ambientlight/bs-rx`, `reason-promise`, `bs-fetch`, `reductive` installed (peer dependencies)

```
npm install reductive-observable @ambientlight/bs-rx reason-promise bs-fetch reductive
```

Then add `reductive-observable`, `@ambientlight/bs-rx`, `reason-promise`, `bs-fetch`, `reductive` into `bs-dependencies` in your project `bsconfig.json`.

## Usage: ReductiveObservable.middleware

Your side-effect is defined as `epic`. Epic is an observable operator (transformer function) that takes an `Rx.Observable.t((action, state))` and returns an `Rx.Observable.t(action)`, that is an observable that emits actions back to the store. Let's look at the following example:

```reason
module State {
  type t = {
    count: int
  };
};

module Action {
  type t = 
    | Increment 
    | Decrement 
    | StartIncrementing(int) 
    | StartDecrementing(int)
};

let reducer = (state: State.t, action: Action.t): State.t => 
  switch(action){
  | Increment => { count: state.count + 1 }
  | Decrement => { count: state.count - 1 }
  | StartIncrementing(_) => state
  | StartDecrementing(_) => state
  };

module Epics {
  let startIncrementing = (ro: Rx.Observable.t((Action.t, State.t))) => ReductiveObservable.Utils.({ 
    ro
    |> optmap(fun | (Action.StartIncrementing(count), _store) => Some(count) | _ => None)
    |> Rx.Operators.mergeMap(`Observable((count, _idx) => 
      Rx.range(~count=count, ())
      |> Rx.Operators.map((_value, _idx) => Action.Increment)
    ))
  });

  let startDecrementing = (ro: Rx.Observable.t((Action.t, State.t))) => ReductiveObservable.Utils.({ 
    ro
    |> optmap(fun | (Action.StartDecrementing(count), _store) => Some(count) | _ => None)
    |> Rx.Operators.mergeMap(`Observable((count, _idx) => 
      Rx.range(~count=count, ())
      |> Rx.Operators.map((_value, _idx) => Action.Decrement)
    ))
  });

  /**
    use empty operator when side effect does not need to emit actions back to the store
   */
  let logState = (ro: Rx.Observable.t((Action.t, State.t))) => 
    ro
    |> Rx.Operators.tap(~next=((_action, state)) => Js.log(state))
    |> ReductiveObservable.Utils.empty;

  let root = (ro: Rx.Observable.t((Action.t, State.t))) =>
    Rx.merge([|
      ro |> startIncrementing,
      ro |> startDecrementing,
      ro |> logState
    |]);
};
```

We have 3 epics defined here. Two of them trasform `StartDecrementing(count) / StartDecrementing(count)` into #count dispatches of `Increment / Decrement` back to the store via `Rx.range` observable creator. The third action just logs the state after each action dispatched, and since it doesn't need to emit anything back to the store, `ReductiveObservable.Utils.empty` operator is used to mergeMap observable sequence to `Rx.empty`.
To apply these epics pass them as parameter to `ReductiveObservable.middleware` which is passed to reductive store creator.

```reason
let store = Reductive.Store.create(
  ~reducer,
  ~preloadedState={ count: 0 },
  ~enhancer=ReductiveObservable.middleware(Rx.of1(Epics.root)),
  ()
);
```

## Usage: ObservableStore

When you need to observe the status or completion of your side effects on the dispatching side, wrap your store into `ObservableStore`, where instead of using `dispatch(yourAction)`, `observe(yourAction)` can be used to return an observable that will emit actions belonging to the same logical action chain.

```reason
// let store = Reductive.Store.create(...)

let obsStore = ObservableStore.create(
  store,
  ~enhancer=ReductiveObservable.middleware(Rx.of1(Epics.progress)),
  ()
)

// obsStore |. Reductive.Store.dispatch(StartLongEffect)

obsStore
|. ObservableStore.observe(StartLongEffect)
|> Rx.Operators.tap(~next=progress => Js.log(progress))
|> Rx.Operators.reduce((progress, action, idx) => switch(action){
  | Action.Update(status) => status
  | Action.EndLongEffect => progress == 100 ? 100 : -1
  | _ => progress
}, 0)
|> Rx.Observable.subscribe(~next=progress => Js.log(progress == 100 ? "done" : "something wrong"))
```

where the Epics and Action is defined as:

```reason
module Action {
  type t = 
    | StartLongEffect
    | Update(int)
    | EndLongEffect
};

module Epics {
  let progress = ro => ReductiveObservable.Utils.({
    ro
    |> optmap(fun 
      | (ObservableStore.Start(Action.StartLongEffect, subject), _store) => Some(subject) 
      | _ => None)
    |> Rx.Operators.mergeMap(`Observable((subject, _idx) =>
      Rx.concat([|
        Rx.range(~count=100, ())
        |> Rx.Operators.map((value, _idx) => value + 1)
        |> Rx.Operators.map((value, _idx) => ObservableStore.Update(Action.Update(value), subject)),
        Rx.of1(ObservableStore.End(Action.EndLongEffect, subject))
      |])
    ))
  });
}
```

The epics you define take `Rx.Observable.t((observableAction('action), 'state))` and return `Rx.Observable.t(observableAction('action))` where observableAction is defined as:

```reason
type observableAction('action) =
  | Start('action, Rx.ReplaySubject.t('action))
  | Update('action, Rx.ReplaySubject.t('action))
  | Error('action, Rx.ReplaySubject.t('action))
  | End('action, Rx.ReplaySubject.t('action));
```

`observableAction` wraps the actions you dispatch into a `status` variant:
 * `Start()` will wrap any actions you pass to `observe()`, these are the actions you want to handle in your epics
 * `Update()` will progagate your action, use it for actions that represent intermediate updates
 * `End()` will propagate the action and complete your observable.
 * `Error()` will error your obsevable with an action you pass (which you might probably want to handle with `Rx.Operators.catchError()`)

As you've also noticed, subject you extract from `Start()` action is passed along. You normally should not need to send actions directly into it. 

### Action Chain

Sequence of actions that will be dispatched to a store that belong to the same side-effect you are modeling, for example:

```reason
let signOut = (reductiveObservable: Rx.Observable.t(('action, 'state))) => Rx.Operators.({
  reductiveObservable
  |> Utils.Rx.optMap(fun | (`SignOutRequest(()), _state) => Some(()) | _ => None)
  |> mergeMap(`Observable((_, _idx) => 
    Rx.merge([|
      Rx.of1(`SignOutStarted(())),
      Rx.from(`Promise(Amplify.Auth.signOut(())), ())
      |> map((_result, _idx) => `SignOutCompleted(()))
      |> catchError((error, _notif) => Rx.of1(`SignOutError(error|.composeError)))
    |])
  ))
})
```

This epic models sign-out with a following action chain dispatched to the store: `SignOutRequest` -> `SignOutStarted` -> `SignOutError/SignOutCompleted`.

### Additional notes

* You can still use your original `store` as you normally do.
* You can also use all reductive APIs on `ObservableStore.t` instances, replace `Reductive.Store.` with `ObservableStore.`
* Use `observe()` with actions you handle in the epics passed to `ObservableStore.create`, if you call `observe()` with actions that are not handled in your epics, the observable will never complete, if you need a saveguard against those cases, use `Rx.Operators.timeout`

## Hot Reload of epics

This middleware supports react HMR. For HMR bindings defined as: 

```reason
type hot;
[@bs.send] external _accept: (hot, string, unit => unit) => unit = "accept";
[@bs.send] external _decline: hot => unit = "decline";

[@bs.deriving abstract]
type module_type = {
  hot: Js.Undefined.t(hot)
};

[@bs.val]
external module_: module_type = "module"; 

let isAvailable = (module_) => 
  module_ 
  |. hotGet 
  |. Js.Undefined.toOption
  |. Belt.Option.isSome;

let accept = (module_, path, onHotReload) => {
  let hmr = module_ |. hotGet;
  switch(hmr |. Js.Undefined.toOption){
  | None => Console.warn("Webpack HMR is not available, accept did nothing")
  | Some(hmr) => _accept(hmr, path, onHotReload);
  };
}

let decline = (module_) => {
  let hmr = module_ |. hotGet;
  switch(hmr |. Js.Undefined.toOption){
  | None => Console.warn("Webpack HMR is not available, decline did nothing")
  | Some(hmr) => { 
    _decline(hmr); 
  }
  };  
}
```

define you epic as `Rx.BehaviourSubject`:

```reason
let hmrEpic = Rx.BehaviorSubject.create(Epics.root);
let store = storeCreator(
  ~reducer=Reducers.root, 
  ~preloadedState=initial, 
  ~enhancer=ReductiveObservable.middleware(hmrEpic |> Rx.BehaviorSubject.asObservable), 
  ());
```

then using HMR functionality:

```reason
if(HMR.isAvailable(HMR.module_)){
  HMR.accept(HMR.module_, "./lib/js/src/reductive/epics/Epics.bs.js", () => {
    let hotReloadedRootEpic: (Rx.Observable.t(('action, 'state))) => Rx.Observable.t(('action)) = [%bs.raw "require('reason/reductive/epics/Epics.bs.js').epic"];
    
    /**
     * this is safe ONLY WHEN epics are stateless
     * given RxJs nature, it's easy to introduce implicit states into epics
     * when using anything utilizing BehaviourSubject/ReplaySubject/shareReplay etc.
     * be VERY CAREFUL with it as it can lead to unpredictable states when hot reloaded
     */
    hmrEpic 
    |> Rx.BehaviorSubject.next(hotReloadedRootEpic);
    Js.log("[HMR] (Store) ReductiveObservable epics hot reloaded");
  });
};
```
