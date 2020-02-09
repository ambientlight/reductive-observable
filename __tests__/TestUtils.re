
let fullfillIn = (callback, observable) => 
  observable 
  |> Rx.Observable.subscribe(
    ~next=fulfilled => callback(fulfilled),
    ~error=failed => callback(failed))
  |> ignore;

let testObservable = (name, observableCallback) => 
  Jest.testAsync(name, callback => 
    observableCallback() 
    |> fullfillIn(callback));