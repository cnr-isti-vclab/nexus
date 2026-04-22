    
 function PriorityQueue(max_length) {
    this.error = new Float32Array(max_length);
    this.data = new Int32Array(max_length);
    this.size = 0;
}

PriorityQueue.prototype = {
    push: function(data, error) {
        this.data[this.size] = data;
        this.error[this.size] = error;
        this.bubbleUp(this.size);
        this.size++;
    },

    pop: function() {
        var result = this.data[0];
        this.size--;
        if(this.size > 0) {
            this.data[0] = this.data[this.size];
            this.error[0] = this.error[this.size];
            this.sinkDown(0);
        }
        return result;
    },

    bubbleUp: function(n) {
        var data = this.data[n];
        var error = this.error[n];
        while (n > 0) {
            var pN = ((n+1)>>1) -1;
            var pError = this.error[pN];
            if(pError > error)
                break;
            //swap
            this.data[n] = this.data[pN];
            this.error[n] = pError;
            this.data[pN] = data;
            this.error[pN] = error;
            n = pN;
        }
    },

    sinkDown: function(n) {
        var data = this.data[n];
        var error = this.error[n];

        while(true) {
            var child2N = (n + 1) * 2;
            var child1N = child2N - 1;
            var swap = -1;
            if (child1N < this.size) {
                var child1Error = this.error[child1N];
                if(child1Error > error)
                    swap = child1N;
            }
            if (child2N < this.size) {
                var child2Error = this.error[child2N];
                if (child2Error > (swap == -1 ? error : child1Error))
                    swap = child2N;
            }

            if (swap == -1) break;

            this.data[n] = this.data[swap];
            this.error[n] = this.error[swap];
            this.data[swap] = data;
            this.error[swap] = error;
            n = swap;
        }
    }
};


export { PriorityQueue }

