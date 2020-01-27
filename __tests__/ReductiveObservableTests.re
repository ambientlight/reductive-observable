open Jest;

module State {
  type t = {
    count: int
  };
};

module Action {
  type t = 
    | Increment 
    | Decrement 
    | StartIncrementing(int, assertion => unit) 
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
    |> optmap(fun | (Action.StartIncrementing(count, callback), _store) => Some((count, callback)) | _ => None)
    |> Rx.Operators.mergeMap(`Observable(((count, callback), _idx) => 
      Rx.range(~count=count, ())
      |> Rx.Operators.map((_value, _idx) => Action.Increment)
      |> Rx.Operators.finalize(() => {
        callback(Expect.expect(true) |> Expect.toBe(true))
      })
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
  let logActions = (ro: Rx.Observable.t((Action.t, State.t))) => 
    ro
    |> Rx.Operators.tap(~next=((_action, state)) => Js.log(state))
    |> ReductiveObservable.Utils.empty;
      
  let root = (ro: Rx.Observable.t((Action.t, State.t))) =>
    Rx.merge([|
      ro |> startIncrementing,
      ro |> startDecrementing,
      ro |> logActions
    |]);
};

describe("ReductiveObservableTests", () => {
  testAsync("basic epics", callback => {
    let store = Reductive.Store.create(
      ~reducer,
      ~preloadedState={ count: 0 },
      ~enhancer=ReductiveObservable.middleware(Rx.of1(Epics.root)),
      ()
    );

    store |. Reductive.Store.dispatch(Action.StartIncrementing(100, callback));
  });

  ()
});