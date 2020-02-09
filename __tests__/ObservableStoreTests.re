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
}

let reducer = (state: State.t, action: Action.t): State.t => 
  switch(action){
  | Foo => { text: state.text ++ "foo" }
  | Bar => { text: state.text ++ "bar" }
  };


describe("ObservableStoreTests", () => {
  open Expect;

  testObservable("basic ObservableStore", () => {
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
      expect(action)
      |> toEqual(Action.Bar))
  });

  ()
});