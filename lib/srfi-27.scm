;;;
;;; srfi-27.scm - Sources of Random Bits
;;;
;;;  Copyright(C) 2001 by Shiro Kawai (shiro@acm.org)
;;;
;;;  Permission to use, copy, modify, distribute this software and
;;;  accompanying documentation for any purpose is hereby granted,
;;;  provided that existing copyright notices are retained in all
;;;  copies and that this notice is included verbatim in all
;;;  distributions.
;;;  This software is provided as is, without express or implied
;;;  warranty.  In no circumstances the author(s) shall be liable
;;;  for any damages arising out of the use of this software.
;;;
;;;  $Id: srfi-27.scm,v 1.1 2002-06-05 22:38:02 shirok Exp $
;;;

;; Implements SRFI-27 interface on top of math.mt-random module.

(define-module srfi-27
  (use math.mt-random)
  (use srfi-4)
  (export random-integer random-real default-random-source
          make-random-source random-source?
          random-source-state-ref random-source-state-set!
          random-source-randomize! random-source-pseudo-randomize!
          random-source-make-integers random-source-make-reals
          ))
(select-module srfi-27)

;; Assumes random source is <mersenne-twister> random object for now.
;; It is possible that I extend the implementation so that users can
;; specify the class of random source in future.
(define-constant random-source <mersenne-twister>)

;; Operations on random source
(define (make-random-source) (make random-source))
(define (random-source? obj) (is-a? obj random-source))
(define default-random-source (make-random-source))

(define (random-source-state-ref source)
  (mt-random-get-state source))
(define (random-source-state-set! source state)
  (mt-random-set-state! source state))

;; Randomize
(define (random-source-randomize! source)
  (unless (random-source? source)
    (error "random source required, but got" source))
  (mt-random-set-seed! source
                       (let1 s (* (sys-time) (sys-getpid))
                         (logior s (ash s -16)))))

(define (random-source-pseudo-randomize! source i j)
  ;; This procedure is effectively required to map integers (i,j) into
  ;; a seed value in a deterministic way.  Talking advantage of the fact
  ;; that Mersenne Twister can take vector of numbers.

  ;; interleave-i and interleave-j creates a list of integers, each
  ;; is less than 2^32, consisted by interleaving each 32-bit chunk of i and j.
  (define (interleave-i i j lis)
    (if (zero? i)
        (if (zero? j) lis (interleave-j 0 j (cons 0 lis)))
        (receive (q r) (quotient&remainder i #x100000000)
          (interleave-j q j (cons r lis)))))

  (define (interleave-j i j lis)
    (if (zero? j)
        (if (zero? i) lis (interleave-i i 0 (cons 0 lis)))
        (receive (q r) (quotient&remainder j #x100000000)
          (interleave-i i q (cons r lis)))))

  ;; main body
  (unless (random-source? source)
    (error "random source required, but got" source))
  (when (or (not (integer? i)) (not (integer? j))
            (negative? i) (negative? j))
    (errorf "indices must be non-negative integers: ~s, ~s" i j))
  (mt-random-set-seed! source
                       (list->u32vector (interleave-i i j '(#xffffffff))))
  )

;; Obtain generators from random source.
(define (random-source-make-integers source)
  (unless (random-source? source)
    (error "random source required, but got" source))
  (lambda (n) (mt-random-integer source n)))

(define (random-source-make-reals source . maybe-unit)
  (unless (random-source? source)
    (error "random source required, but got" source))
  (if (null? maybe-unit)
      (lambda () (mt-random-real source))
      (let1 unit (car maybe-unit)
        (unless (< 0 unit 1)
          (error "unit must be between 0.0 and 1.0 (exclusive), but got" unit))
        (let* ((1/unit (/ unit))
               (range (inexact->exact (ceiling 1/unit))))
          (lambda ()
            (/ (make-random-integer range) 1/unit))))))

;; Default random generators.
(define-values (random-integer random-real)
  (let1 src default-random-source
    (values (lambda (n) (mt-random-integer src n))
            (lambda ()  (mt-random-real src)))))

(provide "srfi-27")
