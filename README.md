# reductive-observable
Minimal port of [redux-observable](https://github.com/redux-observable/redux-observable) to reason reductive.
Centalized rx side-effects for reductive.

## Installation

```
npm install reductive-observable
```

Then add `reductive-observable` into `bs-dependencies` in your project `bsconfig.json`

## Usage

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
