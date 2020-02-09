open Jest;
open TestUtils;

module State {
  type t = {
    text: string
  };
};

module Action {
  type t = 
    | Foo 
    | Bar
    | StartLongEffect
    | Update(int)
    | EndLongEffect
};

module Epics {
  let fooBar = ro => ReductiveObservable.Utils.({
    ro
    // handle Start(Action.Foo, subject) and unwrap subject
    |> optmap(fun 
      | (ObservableStore.Start(Action.Foo, subject), _store) => Some(subject) 
      | _ => None)
    // dispatch End with a same subject instance passed in Start 
    // to signalize a logical end to an action sequence
    |> Rx.Operators.map((subject,_idx) => ObservableStore.End(Action.Bar, subject))
  });

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

let reducer = (state: State.t, action: Action.t): State.t => 
  switch(action){
  | Foo => { text: state.text ++ "foo" }
  | Bar => { text: state.text ++ "bar" }
  | StartLongEffect => { text: "started" }
  | Update(status) => { text: string_of_int(status) }
  | EndLongEffect => { text: "done" }
  };


describe("ObservableStoreTests", () => {
  open Expect;

  testObservable("basic", () => {
    let store = Reductive.Store.create(
      ~reducer,
      ~preloadedState={ text: "" },
      ()
    );

    let obsStore = ObservableStore.create(
      store,
      ~enhancer=ReductiveObservable.middleware(Rx.of1(Epics.fooBar)),
      ()
    )

    obsStore
    |. ObservableStore.observe(Foo)
    |> Rx.Operators.map((action, idx) => 
      expect(
        action == Action.Bar 
        && (obsStore |. ObservableStore.getState).text == "foobar")
      |> toBe(true))
  });

  testObservable("update", () => {
    let store = Reductive.Store.create(
      ~reducer,
      ~preloadedState={ text: "" },
      ()
    );

    let obsStore = ObservableStore.create(
      store,
      ~enhancer=ReductiveObservable.middleware(Rx.of1(Epics.progress)),
      ()
    )

    obsStore
    |. ObservableStore.observe(StartLongEffect)
    |> Rx.Operators.tap(~next=progress => Js.log(progress))
    |> Rx.Operators.reduce((progress, action, idx) => switch(action){
      | Action.Update(status) => status
      | Action.EndLongEffect => progress == 100 ? 100 : -1
      | _ => progress
    }, 0)
    |> Rx.Operators.map((progress, _idx) => expect(progress) |> toEqual(100))
  });

  ()
});